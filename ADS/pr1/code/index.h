/* pr1（8-1 Roll Your Own Mini Search Engine）：倒排索引的公共接口。
 *
 * 文件职责（对应题面的三个程序）：
 *   tokenize.c + tokenize.h  —— 【未实现】原始文本 -> (term, doc_id, pos) 三元组流
 *   wordcount.c              —— 【未实现】Part 1：词/文档频次统计 + θ 阈值 -> stoplist
 *   index_gen.c              —— Part 2：读三元组建内存索引（index_build）+ main（落盘）
 *                               【未实现】按 stoplist 剔词
 *   query.c                  —— Part 3：加载索引（index_load）+ 词查询（query_word）+ main
 *                               【未实现】短语查询、查询阈值 τ
 *   index_io.c               —— 共享索引模块：内存索引的构造/销毁/插入/查找 + index.bin 读写
 *   stem.c / stem.h          —— 词干模块，已就绪（Porter 算法）
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

#include <stdio.h>

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

/* ---------------- index_io.c：内存索引 + 落盘 ---------------- */

/* 堆分配并清零。结构体约 7.6 MB（1000007 × 8 B），必须放堆上，不能放栈上。 */
inverted_index *index_create(void);

/* 释放索引的全部内容以及结构体本身 */
void index_free(inverted_index *index);

/* 文档表插入（二分定位 + 保持升序去重）。
 * 返回 1 = 新文档，0 = 已存在，-1 = 内存不足。 */
int index_add_doc(DocTable *table, int doc_id);

/* 插入一条 (词干, doc_id, pos)，保持 (doc_id, pos) 升序。
 * 返回 1 = 已插入，0 = 完全重复（同一 doc_id + pos）或内存不足。
 * stem 必须已经过 stemword()。index_build() 与 index_load() 共用这一个入口，
 * 所以落盘前/落盘后的去重与排序行为完全一致。 */
int index_add_position(inverted_index *index, const char *stem, int doc_id, int pos);

/* 按词干查 Posting，找不到返回 NULL。word 必须已经过 stemword()。 */
Posting *index_find(inverted_index *index, const char *word);

/* df：命中该词干的文档数（题面第 4 问 df/N 的分子）。
 * 依赖 positions 已按 doc_id 升序。 */
int posting_df(const Posting *posting);

/* 把内存索引写成 index.bin 格式，返回写出的词条数，失败返回 -1。
 * 写出的词条按词干升序，输出与哈希桶顺序无关，可复现、可 diff。 */
long index_save(inverted_index *index, FILE *out);

/* 读 index.bin 格式重建内存索引（含文档表），返回 NULL 表示格式非法/损坏/内存不足。
 * 需要可 seek 的二进制流（普通文件即可），且文件小于 LONG_MAX。 */
inverted_index *index_load(FILE *in);

/* ---------------- index_gen.c：建索引侧 ---------------- */

/* 读 "<word> <doc_id> <pos>" 三元组建索引，返回入库的位置条目数。
 * 词先经 stemword() 词干化；重复的 (词干, doc_id, pos) 会被忽略；
 * doc_id / pos 为负的行会被跳过，并在 stderr 汇总统计。
 * 注意：格式不符的行会让 fscanf 提前返回，其后内容不再读入 —— 要容忍脏输入得自己按行读。 */
long index_build(inverted_index *index, FILE *file);

/* ---------------- query.c：查询侧 ---------------- */

/* 查询单个词，把结果写进 output。不会改写 word。 */
void query_word(inverted_index *index, const char *word, FILE *output);

#endif /* PR1_INDEX_H */
