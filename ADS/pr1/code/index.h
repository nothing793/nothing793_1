/* pr1（8-1 Roll Your Own Mini Search Engine）：共享索引模块（单头文件库）。
 *
 * 本文件既声明接口、也包含实现，两个程序各 #include 一次即可，不需要额外的 .c：
 *
 *   index_gen.c   —— Part 1（词频统计 + θ 阈值 -> stoplist.txt）
 *                    + Part 2（建内存索引 + 落盘 index.bin）+ 切词（原始文本 -> 词序列）
 *   query.c       —— Part 3（加载索引 + 单词 / AND / 短语查询）+ Part 4（查询阈值 τ）
 *   stem.c/stem.h —— 外部引用（Porter 算法），两个程序都要链它（-lm 也一样）
 *
 * 为什么把实现放在头文件里：index_gen（写 index.bin）和 query（读 index.bin）必须对
 * 同一份文件格式达成一致。实现只有这一份、两边都 #include 它，就不可能出现"建的时候
 * 一套、读的时候另一套"。函数全部写成 static inline，所以：
 *   - 每个程序各自编译出一份私有副本，链接时不会重复定义；
 *   - 某个程序用不到的那一半（比如 query 用不到 index_save）不会触发 -Wunused-function。
 *
 * 索引文件 index.bin（版本 1）：小端定长整数 + LEB128 变长整数
 *
 *   头部（固定 64 B）
 *     +0    8 B   魔数 "PR1IDX" + 两个 '\0'
 *     +8    4 B   u32 版本号（1）
 *     +12   4 B   u32 头部长度（64，便于以后扩展）
 *     +16   8 B   u64 文档数
 *     +24   8 B   u64 词条数
 *     +32   8 B   u64 位置条目总数（校验用）
 *     +40   8 B   u64 docs 段文件偏移
 *     +48   8 B   u64 terms 段文件偏移
 *     +56   8 B   u64 postings 段文件偏移
 *
 *   docs 段：文档数 × u32（doc_id，升序去重；给 df/N 提供 N）
 *   terms 段：词条数 × 记录，按词干升序（便于将来落盘二分 / 多路归并）
 *     u32 df、u32 tf、u64 postings 段内偏移、u16 词干字节数、词干字节（不含 '\0'）
 *   postings 段：与 terms 段同序，每个词条一段，全部 LEB128 编码
 *     [doc 增量][该文档的位置数][位置增量 × 位置数] 这样的组重复 df 次
 *       - 第一个 doc 增量 = doc_id 本身（相对 0 算），其后 = doc_id - 上一个 doc_id（≥ 1）
 *       - 位置增量 = pos - 上一个 pos，组内首个 pos 视上一个为 -1（所以增量 ≥ 1）
 *     解码时 doc_id / pos 依次累加，即得按 (doc_id, pos) 升序的位置链；
 *     df = 组数，tf = 各组位置数之和。
 *
 *   df / tf 在 terms 段里是冗余的（能从 postings 段推出来），写这两个字段是为了让 query
 *   不必重算、并和头部总数互相校验。index_load() 会逐项验证（魔数、版本、段偏移、
 *   每个词条的 df/tf、位置总数、文档表），任何不一致都返回 NULL，不会把残缺索引交给查询程序。
 *
 *   doc_id / pos 用 u32 存（写入侧已保证非负且 ≤ INT_MAX），文件偏移用 u64。
 *   这份格式面向真实语料（40 部剧，索引几 MB）；Bonus 规模（4×10^8 词）要换成
 *   分块 + 前缀压缩的 SSTable 词典，见 README.md 第 7.2 节 —— 两者共用同一套接口，只换后端。
 */
#ifndef PR1_INDEX_H
#define PR1_INDEX_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define hashsize 1000007     /* 哈希桶数，取质数减少冲突 */
#define maxword 256          /* 单词缓冲长度，含结尾 '\0' */
#define INDEX_FILE "index.bin"   /* 索引文件：index_gen 写、query 读 */

/* 文档表：记录出现过的 doc_id，升序去重，给 df / N 提供 N。
 * doc_id 不要求连续，也不要求按顺序出现。 */
typedef struct doc_table
{
    int *ids;
    int count;
    int capacity;
} DocTable;

/* 一个 (文档, 位置) 条目，挂在对应词干的 postings 链上 */
typedef struct position
{
    int doc_id;
    int pos;
    struct position *next;
} Position;

typedef struct posting
{
    char *word;            /* 词干（不是原形），strcmp 用 */
    int occurrences;       /* tf：该词干累计的位置条目数 */
    Position *positions;   /* 按 (doc_id, pos) 升序；df、求交、短语验证都依赖有序 */
    struct posting *next;
} Posting;

typedef struct inverted_index
{
    Posting *buckets[hashsize];
    DocTable docs;
} inverted_index;

/* 下面是实现。static inline 的两个作用：同一份头文件被两个 .c 包含时不会重复定义，
 * 以及某个程序用不到的函数不会触发 -Wunused-function（普通 static 会告警）。 */

/* djb2 */
static inline unsigned long hash_word(const char *word)
{
    unsigned long hash = 5381;
    const char *str = word;
    while (*str)
    {
        hash = ((hash << 5) + hash) + (unsigned char)(*str);
        str++;
    }
    return hash % hashsize;
}

/* C99 标准里没有 strdup（那是 POSIX），自己写一个 */
static inline char *dup_string(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy)
    {
        memcpy(copy, s, len);
    }
    return copy;
}

/* ---------------- 内存索引：构造 / 销毁 / 插入 / 查找 ---------------- */

/* 堆分配并清零。结构体约 7.6 MB（1000007 × 8 B），必须放堆上，不能放栈上。 */
static inline inverted_index *index_create(void)
{
    return (inverted_index *)calloc(1, sizeof(inverted_index));
}

/* 释放索引的全部内容以及结构体本身 */
static inline void index_free(inverted_index *index)
{
    if (!index)
    {
        return;
    }
    for (int i = 0; i < hashsize; i++)
    {
        Posting *posting = index->buckets[i];
        while (posting)
        {
            Posting *next_posting = posting->next;
            free(posting->word);
            Position *pos = posting->positions;
            while (pos)
            {
                Position *next_pos = pos->next;
                free(pos);
                pos = next_pos;
            }
            free(posting);
            posting = next_posting;
        }
    }
    free(index->docs.ids);
    free(index);
}

/* 按词干查 Posting，找不到返回 NULL。word 必须已经过 stemword()。 */
static inline Posting *index_find(inverted_index *index, const char *word)
{
    Posting *posting = index->buckets[hash_word(word)];
    while (posting && strcmp(posting->word, word) != 0)
    {
        posting = posting->next;
    }
    return posting;
}

/* 文档表插入（二分定位 + 保持升序去重）。
 * 返回 1 = 新文档，0 = 已存在，-1 = 内存不足。 */
static inline int index_add_doc(DocTable *table, int doc_id)
{
    int lo = 0, hi = table->count;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (table->ids[mid] < doc_id)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo < table->count && table->ids[lo] == doc_id)
    {
        return 0;
    }

    if (table->count == table->capacity)
    {
        int new_capacity = table->capacity ? table->capacity * 2 : 64;
        int *grown = (int *)realloc(table->ids, (size_t)new_capacity * sizeof(int));
        if (!grown)
        {
            return -1;
        }
        table->ids = grown;
        table->capacity = new_capacity;
    }

    memmove(&table->ids[lo + 1], &table->ids[lo], (size_t)(table->count - lo) * sizeof(int));
    table->ids[lo] = doc_id;
    table->count++;
    return 1;
}

/* 插入一条 (词干, doc_id, pos)，保持 (doc_id, pos) 升序。
 * 返回 1 = 已插入，0 = 完全重复（同一 doc_id + pos）或内存不足。
 * stem 必须已经过 stemword()。index_gen 建索引与 index_load() 读盘共用这一个入口，
 * 所以落盘前/落盘后的去重与排序行为完全一致。 */
static inline int index_add_position(inverted_index *index, const char *stem, int doc_id, int pos)
{
    unsigned long hash = hash_word(stem);
    Posting *posting = index_find(index, stem);

    if (!posting)
    {
        posting = (Posting *)malloc(sizeof(Posting));
        if (!posting)
        {
            return 0;
        }
        posting->word = dup_string(stem);
        if (!posting->word)
        {
            free(posting);
            return 0;
        }
        posting->occurrences = 0;
        posting->positions = NULL;
        posting->next = index->buckets[hash];
        index->buckets[hash] = posting;
    }

    Position **link = &posting->positions;
    while (*link
           && ((*link)->doc_id < doc_id
               || ((*link)->doc_id == doc_id && (*link)->pos < pos)))
    {
        link = &(*link)->next;
    }
    if (*link && (*link)->doc_id == doc_id && (*link)->pos == pos)
    {
        return 0;   /* 重复条目，不插 */
    }

    Position *node = (Position *)malloc(sizeof(Position));
    if (!node)
    {
        return 0;
    }
    node->doc_id = doc_id;
    node->pos = pos;
    node->next = *link;
    *link = node;
    posting->occurrences++;
    return 1;
}

/* df：命中该词干的文档数（题面第 4 问 df/N 的分子）。
 * 依赖 positions 已按 doc_id 升序。 */
static inline int posting_df(const Posting *posting)
{
    int df = 0, last_doc = -1;
    for (const Position *pos = posting->positions; pos; pos = pos->next)
    {
        if (pos->doc_id != last_doc)
        {
            df++;
            last_doc = pos->doc_id;
        }
    }
    return df;
}

/* ---------------- 变长字节缓冲（编码的唯实现，量长度和写出共用） ---------------- */

typedef struct byte_buf
{
    unsigned char *data;
    size_t len;
    size_t capacity;
} ByteBuf;

static inline int buf_reserve(ByteBuf *buf, size_t extra)
{
    if (buf->len + extra <= buf->capacity)
    {
        return 0;
    }
    size_t capacity = buf->capacity ? buf->capacity : 256;
    while (capacity < buf->len + extra)
    {
        capacity *= 2;
    }
    unsigned char *grown = (unsigned char *)realloc(buf->data, capacity);
    if (!grown)
    {
        return -1;
    }
    buf->data = grown;
    buf->capacity = capacity;
    return 0;
}

static inline int buf_put_byte(ByteBuf *buf, unsigned char byte)
{
    if (buf_reserve(buf, 1) < 0)
    {
        return -1;
    }
    buf->data[buf->len++] = byte;
    return 0;
}

/* LEB128：每字节低 7 位有效，最高位为 1 表示"还有后续" */
static inline int buf_put_varint(ByteBuf *buf, unsigned long value)
{
    do
    {
        unsigned char byte = (unsigned char)(value & 0x7F);
        value >>= 7;
        if (value)
        {
            byte |= 0x80;
        }
        if (buf_put_byte(buf, byte) < 0)
        {
            return -1;
        }
    } while (value);
    return 0;
}

/* 把一个词干的位置链编码进 buf（buf->len 先清零）。
 * 位置链已按 (doc_id, pos) 升序，所以 doc 增量恒 ≥ 1（首个除外），pos 增量也恒 ≥ 1。
 * 返回 0 成功，-1 表示位置链不是升序/内存不足（说明索引本身坏了，别写出坏文件）。 */
static inline int encode_postings(const Posting *posting, ByteBuf *buf)
{
    const Position *pos = posting->positions;
    int prev_doc = 0;
    int first = 1;

    buf->len = 0;
    while (pos)
    {
        const Position *scan = pos;
        unsigned long count = 0;
        int doc = pos->doc_id;
        int prev_pos = -1;

        if (doc < prev_doc || (!first && doc == prev_doc) || doc < 0)
        {
            return -1;
        }
        while (scan && scan->doc_id == doc)
        {
            if (scan->pos <= prev_pos)
            {
                return -1;   /* 同一文档内 pos 必须递增 */
            }
            prev_pos = scan->pos;
            count++;
            scan = scan->next;
        }

        if (buf_put_varint(buf, (unsigned long)(doc - prev_doc)) < 0
            || buf_put_varint(buf, count) < 0)
        {
            return -1;
        }
        prev_pos = -1;
        while (pos && pos->doc_id == doc)
        {
            if (buf_put_varint(buf, (unsigned long)(pos->pos - prev_pos)) < 0)
            {
                return -1;
            }
            prev_pos = pos->pos;
            pos = pos->next;
        }
        prev_doc = doc;
        first = 0;
    }
    return 0;
}

/* ---------------- 固定宽度小端整数 ---------------- */

/* 魔数固定 8 字节（"PR1IDX" + 两个 '\0'）。写成数组而不是字符串字面量：
 * 字面量长度为 7，按 8 字节读写会越界读到字面量之后的字节。 */
static const unsigned char IDX_MAGIC[8] = {'P', 'R', '1', 'I', 'D', 'X', '\0', '\0'};
#define IDX_VERSION  1
#define IDX_HEADER   64

static inline int put_u16(FILE *out, unsigned long value)
{
    unsigned char buf[2];
    buf[0] = (unsigned char)(value & 0xFF);
    buf[1] = (unsigned char)((value >> 8) & 0xFF);
    return fwrite(buf, 1, sizeof(buf), out) == sizeof(buf) ? 0 : -1;
}

static inline int put_u32(FILE *out, unsigned long value)
{
    unsigned char buf[4];
    buf[0] = (unsigned char)(value & 0xFF);
    buf[1] = (unsigned char)((value >> 8) & 0xFF);
    buf[2] = (unsigned char)((value >> 16) & 0xFF);
    buf[3] = (unsigned char)((value >> 24) & 0xFF);
    return fwrite(buf, 1, sizeof(buf), out) == sizeof(buf) ? 0 : -1;
}

static inline int put_u64(FILE *out, unsigned long long value)
{
    unsigned char buf[8];
    int i;
    for (i = 0; i < 8; i++)
    {
        buf[i] = (unsigned char)((value >> (8 * i)) & 0xFF);
    }
    return fwrite(buf, 1, sizeof(buf), out) == sizeof(buf) ? 0 : -1;
}

static inline int get_u16(FILE *in, unsigned long *value)
{
    unsigned char buf[2];
    if (fread(buf, 1, sizeof(buf), in) != sizeof(buf))
    {
        return -1;
    }
    *value = (unsigned long)buf[0] | ((unsigned long)buf[1] << 8);
    return 0;
}

static inline int get_u32(FILE *in, unsigned long *value)
{
    unsigned char buf[4];
    if (fread(buf, 1, sizeof(buf), in) != sizeof(buf))
    {
        return -1;
    }
    *value = (unsigned long)buf[0] | ((unsigned long)buf[1] << 8)
             | ((unsigned long)buf[2] << 16) | ((unsigned long)buf[3] << 24);
    return 0;
}

static inline int get_u64(FILE *in, unsigned long long *value)
{
    unsigned char buf[8];
    int i;
    unsigned long long result = 0;
    if (fread(buf, 1, sizeof(buf), in) != sizeof(buf))
    {
        return -1;
    }
    for (i = 7; i >= 0; i--)
    {
        result = (result << 8) | (unsigned long long)buf[i];
    }
    *value = result;
    return 0;
}

static inline int get_uvarint(FILE *in, unsigned long *value)
{
    unsigned long result = 0;
    int shift = 0;
    for (;;)
    {
        int byte = fgetc(in);
        if (byte == EOF)
        {
            return -1;
        }
        result |= (unsigned long)(byte & 0x7F) << shift;
        if (!(byte & 0x80))
        {
            break;
        }
        shift += 7;
        if (shift > 63)
        {
            return -1;   /* 超过 64 位：文件损坏 */
        }
    }
    *value = result;
    return 0;
}

static inline int file_size(FILE *in, long long *size)
{
    long here = ftell(in);
    long end;
    if (here < 0 || fseek(in, 0, SEEK_END) != 0)
    {
        return -1;
    }
    end = ftell(in);
    if (end < 0 || fseek(in, here, SEEK_SET) != 0)
    {
        return -1;
    }
    *size = (long long)end;
    return 0;
}

/* ---------------- 落盘：index_save() ---------------- */

static inline int cmp_posting_ptr(const void *a, const void *b)
{
    const Posting *pa = *(Posting *const *)a;
    const Posting *pb = *(Posting *const *)b;
    return strcmp(pa->word, pb->word);
}

/* 把内存索引写成 index.bin 格式，返回写出的词条数，失败返回 -1。
 * 写出的词条按词干升序，输出与哈希桶顺序无关，可复现、可 diff。 */
static inline long index_save(inverted_index *index, FILE *out)
{
    Posting **terms = NULL;
    unsigned long long *offsets = NULL;
    unsigned long *lengths = NULL;
    ByteBuf buf = {NULL, 0, 0};
    long nterm = 0;
    int i;
    unsigned long long ndocs = (unsigned long long)index->docs.count;
    unsigned long long npostings = 0;
    unsigned long long docs_off, terms_off, postings_off, cursor, terms_bytes;
    long result = -1;

    /* 1. 收集全部词干指针并统计位置总数 */
    for (i = 0; i < hashsize; i++)
    {
        for (Posting *posting = index->buckets[i]; posting; posting = posting->next)
        {
            nterm++;
            npostings += (unsigned long long)posting->occurrences;
        }
    }

    terms = (Posting **)malloc((size_t)(nterm > 0 ? nterm : 1) * sizeof(Posting *));
    offsets = (unsigned long long *)malloc((size_t)(nterm > 0 ? nterm : 1) * sizeof(unsigned long long));
    lengths = (unsigned long *)malloc((size_t)(nterm > 0 ? nterm : 1) * sizeof(unsigned long));
    if (!terms || !offsets || !lengths)
    {
        goto done;
    }

    nterm = 0;
    for (i = 0; i < hashsize; i++)
    {
        for (Posting *posting = index->buckets[i]; posting; posting = posting->next)
        {
            terms[nterm++] = posting;
        }
    }
    /* 按词干排序：输出顺序与哈希桶无关，同一份输入永远得到同一个文件 */
    qsort(terms, (size_t)nterm, sizeof(Posting *), cmp_posting_ptr);

    /* 2. 先编码一遍只为量出每个词条的 postings 段长度与偏移 */
    docs_off = IDX_HEADER;
    terms_off = docs_off + 4ULL * ndocs;
    terms_bytes = 0;
    for (long k = 0; k < nterm; k++)
    {
        size_t word_len = strlen(terms[k]->word);
        if (word_len == 0 || word_len > maxword - 1)
        {
            goto done;
        }
        if (encode_postings(terms[k], &buf) < 0)
        {
            goto done;
        }
        lengths[k] = (unsigned long)buf.len;
        terms_bytes += 4ULL + 4ULL + 8ULL + 2ULL + (unsigned long long)word_len;
    }
    postings_off = terms_off + terms_bytes;
    cursor = postings_off;
    for (long k = 0; k < nterm; k++)
    {
        offsets[k] = cursor;
        cursor += (unsigned long long)lengths[k];
    }

    /* 3. 头部 */
    if (fwrite(IDX_MAGIC, 1, 8, out) != 8)
    {
        goto done;
    }
    if (put_u32(out, IDX_VERSION) < 0 || put_u32(out, IDX_HEADER) < 0
        || put_u64(out, ndocs) < 0 || put_u64(out, (unsigned long long)nterm) < 0
        || put_u64(out, npostings) < 0 || put_u64(out, docs_off) < 0
        || put_u64(out, terms_off) < 0 || put_u64(out, postings_off) < 0)
    {
        goto done;
    }

    /* 4. docs 段 */
    for (int k = 0; k < index->docs.count; k++)
    {
        if (put_u32(out, (unsigned long)index->docs.ids[k]) < 0)
        {
            goto done;
        }
    }

    /* 5. terms 段（词典） */
    for (long k = 0; k < nterm; k++)
    {
        Posting *posting = terms[k];
        if (put_u32(out, (unsigned long)posting_df(posting)) < 0
            || put_u32(out, (unsigned long)posting->occurrences) < 0
            || put_u64(out, offsets[k]) < 0
            || put_u16(out, (unsigned long)strlen(posting->word)) < 0
            || fwrite(posting->word, 1, strlen(posting->word), out) != strlen(posting->word))
        {
            goto done;
        }
    }

    /* 6. postings 段（再编码一次写出；顺带核对偏移与量长度那一遍一致） */
    for (long k = 0; k < nterm; k++)
    {
        if ((unsigned long long)ftell(out) != offsets[k])
        {
            goto done;   /* 内部不一致，宁可不产出索引文件 */
        }
        if (encode_postings(terms[k], &buf) < 0
            || (unsigned long)buf.len != lengths[k]
            || fwrite(buf.data, 1, buf.len, out) != buf.len)
        {
            goto done;
        }
    }
    if (ferror(out))
    {
        goto done;
    }

    result = nterm;

done:
    free(buf.data);
    free(lengths);
    free(offsets);
    free(terms);
    return result;
}

/* ---------------- 加载：index_load() ---------------- */

/* 读 index.bin 格式重建内存索引（含文档表），返回 NULL 表示格式非法/损坏/内存不足。
 * 需要可 seek 的二进制流（普通文件即可），且文件小于 LONG_MAX。 */
static inline inverted_index *index_load(FILE *in)
{
    inverted_index *index = NULL;
    unsigned char magic[8];
    unsigned long version = 0, header_size = 0, word_len = 0;
    unsigned long long ndocs = 0, nterms = 0, npostings = 0;
    unsigned long long docs_off = 0, terms_off = 0, postings_off = 0;
    unsigned long long max_doc = 0, decoded = 0, prev_end;
    long long size = 0;

    if (file_size(in, &size) < 0 || size < IDX_HEADER)
    {
        goto fail;
    }
    if (fseek(in, 0, SEEK_SET) != 0 || fread(magic, 1, 8, in) != 8)
    {
        goto fail;
    }
    if (memcmp(magic, IDX_MAGIC, sizeof(IDX_MAGIC)) != 0)
    {
        goto fail;   /* 不是索引文件（可能是旧的文本索引） */
    }
    if (get_u32(in, &version) < 0 || get_u32(in, &header_size) < 0)
    {
        goto fail;
    }
    if (version != IDX_VERSION || header_size != IDX_HEADER)
    {
        goto fail;   /* 版本不匹配：重新跑 index_gen，别硬猜格式 */
    }
    if (get_u64(in, &ndocs) < 0 || get_u64(in, &nterms) < 0 || get_u64(in, &npostings) < 0
        || get_u64(in, &docs_off) < 0 || get_u64(in, &terms_off) < 0
        || get_u64(in, &postings_off) < 0)
    {
        goto fail;
    }

    /* 段偏移必须递增、落在文件内，且各段的长度不能超出下一段的起点。
     * 没有这些检查，一个损坏的文件会让我们按垃圾数字分配内存 / 一直读到天荒地老。 */
    if (docs_off < IDX_HEADER || terms_off < docs_off || postings_off < terms_off
        || postings_off > (unsigned long long)size)
    {
        goto fail;
    }
    if (docs_off + 4ULL * ndocs > terms_off)
    {
        goto fail;
    }
    if (nterms > (postings_off - terms_off) / 18ULL)   /* 每条词条记录至少 18 B */
    {
        goto fail;
    }

    index = index_create();
    if (!index)
    {
        goto fail;
    }

    /* docs 段 */
    if (fseek(in, (long)docs_off, SEEK_SET) != 0)
    {
        goto fail;
    }
    for (unsigned long long k = 0; k < ndocs; k++)
    {
        unsigned long doc_id = 0;
        if (get_u32(in, &doc_id) < 0 || doc_id > (unsigned long)INT_MAX
            || index_add_doc(&index->docs, (int)doc_id) != 1)
        {
            goto fail;   /* 文件里的 doc_id 必须升序去重 */
        }
    }

    /* terms 段 + postings 段 */
    if (fseek(in, (long)terms_off, SEEK_SET) != 0)
    {
        goto fail;
    }
    prev_end = postings_off;
    for (unsigned long long k = 0; k < nterms; k++)
    {
        unsigned long df = 0, tf = 0;
        unsigned long long off = 0;
        unsigned long long group, total;
        unsigned long doc;
        char word[maxword];
        long record_start = ftell(in), record_end;
        long postings_end;

        if (record_start < 0 || get_u32(in, &df) < 0 || get_u32(in, &tf) < 0
            || get_u64(in, &off) < 0 || get_u16(in, &word_len) < 0)
        {
            goto fail;
        }
        if (word_len == 0 || word_len > maxword - 1
            || fread(word, 1, (size_t)word_len, in) != (size_t)word_len
            || memchr(word, '\0', (size_t)word_len) != NULL)
        {
            goto fail;
        }
        word[word_len] = '\0';
        record_end = ftell(in);
        if (record_end < 0 || (unsigned long long)record_end > postings_off)
        {
            goto fail;   /* 词条记录越过了 postings 段起点 */
        }

        /* postings 段：词典里的偏移必须递增、不重叠，且落在文件内 */
        if (off < prev_end || off + 1ULL > (unsigned long long)size
            || fseek(in, (long)off, SEEK_SET) != 0)
        {
            goto fail;
        }
        doc = 0;
        total = 0;
        for (group = 0; group < df; group++)
        {
            unsigned long gap = 0, count = 0, prev = 0, j;
            if (get_uvarint(in, &gap) < 0)
            {
                goto fail;
            }
            doc += gap;
            if ((group > 0 && gap == 0) || doc > (unsigned long)INT_MAX || tf == 0)
            {
                goto fail;   /* doc_id 必须严格递增 */
            }
            if (get_uvarint(in, &count) < 0 || count == 0 || total + count > tf)
            {
                goto fail;
            }
            for (j = 0; j < count; j++)
            {
                unsigned long pos_gap = 0;
                if (get_uvarint(in, &pos_gap) < 0 || pos_gap == 0)
                {
                    goto fail;
                }
                prev += pos_gap;
                if (prev > (unsigned long)INT_MAX + 1UL)
                {
                    goto fail;
                }
                /* 组内首个 pos 记成 pos+1，所以这里减 1 还原 */
                if (index_add_position(index, word, (int)doc, (int)(prev - 1)) != 1)
                {
                    goto fail;
                }
            }
            total += count;
        }
        if (total != tf)
        {
            goto fail;   /* 词典里的 tf 与实际位置数不符 */
        }
        if (df > 0 && doc > max_doc)
        {
            max_doc = doc;
        }
        decoded += total;

        /* 这一段 postings 到此结束：记下结束位置，下一段的偏移必须 ≥ 它（不许重叠），
         * 再回到词典记录的末尾接着读下一条词条。 */
        postings_end = ftell(in);
        if (postings_end < 0 || fseek(in, record_end, SEEK_SET) != 0)
        {
            goto fail;
        }
        prev_end = (unsigned long long)postings_end;
    }

    /* 交叉校验：位置总数、段边界、文档表与位置链必须自洽 */
    if (decoded != npostings)
    {
        goto fail;
    }
    if (prev_end != (unsigned long long)size)
    {
        goto fail;   /* postings 段之后不该有多余字节 */
    }
    if (ndocs == 0 ? decoded != 0
                   : (max_doc != (unsigned long long)index->docs.ids[index->docs.count - 1]))
    {
        goto fail;
    }
    return index;

fail:
    index_free(index);
    return NULL;
}

#endif /* PR1_INDEX_H */
