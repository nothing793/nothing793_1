/* pr1 Part 1 + Part 2：统计 + 建索引程序（原 tokenize.c / tokenize_main.c 已并入本文件）。
 *
 * 一个程序做三件事：
 *
 *   切词 —— 直接读原始文本（每个输入文件 = 一篇文档，doc_id 按命令行顺序 0,1,2...），
 *           按 isalnum 连续段切词、转小写，pos 是词在文档内的出现序号（从 0 起）。
 *           本层**不做词干化**（理由见下面"切词口径"那段）。
 *           原来这一步是独立的 ./tokenize 程序 + file.txt 中间产物，现已并入这里：
 *           少一个可执行文件、少一次落盘，也不会再出现"改了语料忘了重跑 ./tokenize"的陈旧输入。
 *
 *   Part 1（词频统计 + 停用词识别）：
 *     pass 1：把整份语料插进内存倒排索引（顺手得到 cf / df）
 *             逐词条算 df / N，> θ 的判为停用词（noisy words），落盘 stoplist.txt。
 *     θ 的来源：编译期宏 STOPWORD_THETA（默认 0.5，可用 -DSTOPWORD_THETA=0.4 覆盖），
 *              运行时还可以用 --theta=0.4 覆盖宏。
 *
 *   Part 2（带词干化的倒排索引，不含停用词）：
 *     pass 2：把语料再读一遍，跳过停用词，重建索引，写 index.bin。
 *
 * 为什么要扫两遍：df 是"整份语料"的统计量，N 只有扫完才知道；而题面明写索引里
 * 不能包含 Part 1 认定的停用词，所以必须先统计完再建索引。两遍都用同一个
 * tokenize_document() / index_add_position() 入口，因此统计口径与建索引口径不可能不一致。
 * （代价：读语料两遍；原始文本比三元组流小，所以比原来"写一遍 file.txt 再读两遍"更省 I/O。）
 *
 * 用法：
 *   ./index_gen macbeth.txt hamlet.txt            # 每部剧一个文件，θ = STOPWORD_THETA，写 index.bin
 *   ./index_gen --theta=0.4 macbeth.txt          # 换阈值
 *   ./index_gen --count-only macbeth.txt         # 只做 Part 1：打印词频报告 + 写 stoplist.txt
 *   ./index_gen --dump-tokens=file.txt macbeth.txt    # 顺便把三元组写出来（测试对拍用）
 *   ./index_gen -                                # 从 stdin 读一篇文档（doc_id = 0）
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o index_gen index_gen.c stem.c -lm
 *       （索引模块在 index.h 里，本文件 #include 它即可，没有单独的 index_io.c）
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index.h"
#include "stem.h"

/* 判停用词的阈值：df / N > STOPWORD_THETA 的词判为 noisy。
 * 直接改这里的数值，或者编译时用 -DSTOPWORD_THETA=0.4 覆盖，或者运行时 --theta=0.4。 */
#ifndef STOPWORD_THETA
#define STOPWORD_THETA 0.5
#endif

#define STOPLIST_FILE "stoplist.txt"
#define DUMP_FILE "file.txt"     /* --dump-tokens 不给文件名时的默认输出 */

/* ---------------- 小工具 ---------------- */

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy)
    {
        memcpy(copy, s, n);
    }
    return copy;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* ---------------- 切词（原 tokenize.c，已并入本文件） ---------------- */

/* 单个词的最大字节数（含结尾 '\0'）。与 index.h 的 maxword 保持一致，
 * 让"切词"与"索引里的词干缓冲"对同一个词的边界不会出现分歧。 */
#define TOKEN_MAX 256

/* 切词口径（报告里引用这一条即可）：
 *   1. 按 isalnum() 连续段切词，其余字符（空白、换行、标点）一律是分隔符：
 *      "don't" -> "don" / "t"；"well-known" -> "well" / "known"；
 *   2. 纯数字串也当词处理（题面允许）；
 *   3. 每个词只做 tolower()，保留原形（见下面的约定）；
 *   4. pos 是词在文档内的出现序号，从 0 开始、按出现顺序递增；同一个词重复出现各占一个
 *      pos（短语查询的连续性验证依赖这个编号连续）；
 *   5. 超过 TOKEN_MAX-1 字节的超长 isalnum 段按 TOKEN_MAX-1 截断，不报错也不跳过。
 *
 * ⚠ 这一层绝不做词干化。词干化全流程只允许发生一次：建索引时 stemword() 一次、
 *    查询时 stemword() 一次。如果切词时就把词干写进中间产物，后面会再切一次，
 *    而 Porter 不幂等 —— 实测 604 个词里有 11 个二次切会变
 *    （because -> becaus -> becau、release -> releas -> relea、license -> licens -> licen），
 *    于是索引里存 "becau"、查询侧得 "becaus"：查不到，而且从 index.bin 本身看不出异常。
 */

/* ---------------- Part 1：统计与 stoplist ---------------- */

/* 一个词条的统计量（term 指向索引内部的词干，不拥有） */
typedef struct term_stat
{
    const char *term;
    int df;
    int tf;
} term_stat;

/* 停用词表：升序的词干数组 + 二分查找 */
typedef struct stoplist
{
    char **terms;
    int count;
    int capacity;
} stoplist;

static int stoplist_contains(const stoplist *sl, const char *stem)
{
    int lo = 0, hi = sl->count - 1;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        int c = strcmp(stem, sl->terms[mid]);
        if (c == 0)
        {
            return 1;
        }
        if (c < 0)
        {
            hi = mid - 1;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return 0;
}

static int stoplist_push(stoplist *sl, const char *term)
{
    if (sl->count == sl->capacity)
    {
        int cap = sl->capacity ? sl->capacity * 2 : 16;
        char **grown = realloc(sl->terms, (size_t)cap * sizeof(char *));
        if (!grown)
        {
            return -1;
        }
        sl->terms = grown;
        sl->capacity = cap;
    }
    char *copy = dup_str(term);
    if (!copy)
    {
        return -1;
    }
    sl->terms[sl->count++] = copy;
    return 0;
}

static void stoplist_free(stoplist *sl)
{
    for (int i = 0; i < sl->count; i++)
    {
        free(sl->terms[i]);
    }
    free(sl->terms);
    sl->terms = NULL;
    sl->count = sl->capacity = 0;
}

/* 把索引里的全部词条收成一个数组（用于报告与 stoplist 输出；term 不复制） */
static term_stat *collect_stats(const inverted_index *index, long *out_count)
{
    long v = 0;
    for (int i = 0; i < hashsize; i++)
    {
        for (const Posting *p = index->buckets[i]; p; p = p->next)
        {
            v++;
        }
    }

    term_stat *stats = malloc((size_t)(v > 0 ? v : 1) * sizeof(term_stat));
    if (!stats)
    {
        *out_count = 0;
        return NULL;
    }

    long n = 0;
    for (int i = 0; i < hashsize; i++)
    {
        for (const Posting *p = index->buckets[i]; p; p = p->next)
        {
            stats[n].term = p->word;
            stats[n].df = posting_df(p);
            stats[n].tf = p->occurrences;
            n++;
        }
    }
    *out_count = n;
    return stats;
}

static int cmp_stat_df_desc(const void *a, const void *b)
{
    const term_stat *x = a, *y = b;
    if (x->df != y->df)
    {
        return y->df - x->df;
    }
    if (x->tf != y->tf)
    {
        return y->tf - x->tf;
    }
    return strcmp(x->term, y->term);
}

static int cmp_stat_tf_desc(const void *a, const void *b)
{
    const term_stat *x = a, *y = b;
    if (x->tf != y->tf)
    {
        return y->tf - x->tf;
    }
    if (x->df != y->df)
    {
        return y->df - x->df;
    }
    return strcmp(x->term, y->term);
}

/* 按 df / N > θ 挑出停用词，返回条数；表内按词干升序（供二分查找） */
static int stoplist_build(const inverted_index *index, double theta, stoplist *sl)
{
    int total_docs = index->docs.count;
    if (total_docs <= 0)
    {
        return 0;   /* 空语料：没有停用词 */
    }
    for (int i = 0; i < hashsize; i++)
    {
        for (const Posting *p = index->buckets[i]; p; p = p->next)
        {
            if ((double)posting_df(p) / (double)total_docs > theta)
            {
                if (stoplist_push(sl, p->word) < 0)
                {
                    fprintf(stderr, "out of memory while building the stoplist\n");
                    return -1;
                }
            }
        }
    }
    qsort(sl->terms, (size_t)sl->count, sizeof(char *), cmp_str);
    return sl->count;
}

/* 把 stoplist 写成文本文件：给报告用，也让 query 能解释"Not found 是不是停用词"。
 * 注意：它只是 Part 1 的产物与解释材料，**不参与**查询正确性 ——
 * 索引里到底有哪些词由 index.bin 决定。 */
static int stoplist_save(const char *path, const term_stat *stats, long v,
                        const stoplist *sl, double theta, int total_docs)
{
    FILE *out = fopen(path, "w");
    if (!out)
    {
        fprintf(stderr, "Error creating %s\n", path);
        return -1;
    }

    fprintf(out, "# stoplist: theta=%.3f  N=%d  V=%ld  terms=%d\n",
            theta, total_docs, v, sl->count);
    fprintf(out, "# 由 index_gen 生成（Part 1）。仅供报告与解释使用，查询正确性只取决于 index.bin\n");
    fprintf(out, "# term cf df df/N\n");

    /* stats 已按 df 降序，所以这里天然按"噪声程度"排列 */
    for (long i = 0; i < v; i++)
    {
        if (stoplist_contains(sl, stats[i].term))
        {
            double ratio = total_docs > 0 ? (double)stats[i].df / (double)total_docs : 0.0;
            fprintf(out, "%s %d %d %.4f\n", stats[i].term, stats[i].tf, stats[i].df, ratio);
        }
    }

    int failed = (fclose(out) != 0);
    if (failed)
    {
        fprintf(stderr, "Error writing %s\n", path);
        remove(path);
        return -1;
    }
    return 0;
}

/* Part 1 的报告（--count-only 时打印到 stdout，也是报告里"第 1 问"的原始数据） */
static void wc_report(const inverted_index *index, term_stat *stats, long v,
                      const stoplist *sl, double theta)
{
    int total_docs = index->docs.count;
    long positions = 0;
    for (long i = 0; i < v; i++)
    {
        positions += stats[i].tf;
    }

    printf("== Part 1: word count ==\n");
    printf("documents (N)      : %d\n", total_docs);
    printf("distinct stems (V) : %ld\n", v);
    printf("position entries   : %ld\n", positions);
    printf("theta              : %.3f  (df/N > theta -> stop word)\n", theta);

    printf("\n== stop words: %d / %ld (%.1f%%) ==\n", sl->count, v,
           v > 0 ? 100.0 * (double)sl->count / (double)v : 0.0);
    printf("%-20s %10s %10s %8s\n", "stem", "cf", "df", "df/N");
    for (long i = 0; i < v; i++)   /* stats 按 df 降序 */
    {
        if (stoplist_contains(sl, stats[i].term))
        {
            printf("%-20s %10d %10d %8.4f\n", stats[i].term, stats[i].tf, stats[i].df,
                   total_docs > 0 ? (double)stats[i].df / (double)total_docs : 0.0);
        }
    }

    /* 这一段按 cf 排序；wc_report() 是 stats 的最后一位使用者，所以就地排序不会影响别人 */
    qsort(stats, (size_t)v, sizeof(term_stat), cmp_stat_tf_desc);
    long shown = v < 50 ? v : 50;
    printf("\n== top %ld terms by cf (total occurrences) ==\n", shown);
    printf("%-20s %10s %10s %8s\n", "stem", "cf", "df", "df/N");
    for (long i = 0; i < shown; i++)
    {
        printf("%-20s %10d %10d %8.4f%s\n", stats[i].term, stats[i].tf, stats[i].df,
               total_docs > 0 ? (double)stats[i].df / (double)total_docs : 0.0,
               stoplist_contains(sl, stats[i].term) ? "   [stop]" : "");
    }
}

/* ---------------- 文档来源与两遍扫描 ---------------- */

/* 一篇输入文档的来源：命令行给的路径，或者是 stdin（先抄进临时文件，因为要读两遍）。
 * 每次 run_pass() 都从头读，所以 doc_id 与 pos 两遍完全一致。 */
typedef struct doc_source
{
    const char *path;   /* "-" 表示 stdin */
    FILE *spool;        /* path == "-" 时：stdin 的内容抄在这里，两遍各 rewind 一次 */
} doc_source;

/* 把 stdin 抄进一个临时文件（tmpfile 可 seek，正好能读两遍）。
 * 注意：多次给 "-" 只有第一次有内容 —— stdin 读完就没了，后面几次是空文档。 */
static int spool_stdin(doc_source *src)
{
    FILE *tmp = tmpfile();
    if (!tmp)
    {
        fprintf(stderr, "Error creating a temporary file for stdin\n");
        return -1;
    }

    char buffer[65536];
    size_t got;
    while ((got = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
    {
        if (fwrite(buffer, 1, got, tmp) != got)
        {
            fprintf(stderr, "Error copying stdin\n");
            fclose(tmp);
            return -1;
        }
    }
    if (ferror(stdin) || fflush(tmp) != 0 || fseek(tmp, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error copying stdin\n");
        fclose(tmp);
        return -1;
    }
    src->spool = tmp;
    return 0;
}

/* 落一个已经切好的词：先原样写进 dump（测试对拍用的词形，未词干化），
 * 再词干化、必要时按 skip 跳过、最后插进索引。
 * 返回 0 成功，-1 表示 dump 写失败。filtered / dropped 由调用方累加。 */
static int store_word(inverted_index *index, const char *word, int doc_id, int pos,
                      const stoplist *skip, FILE *dump, long *filtered, long *dropped)
{
    if (dump && fprintf(dump, "%s %d %d\n", word, doc_id, pos) < 0)
    {
        return -1;
    }

    char stem[maxword];
    size_t len = strlen(word);
    if (len > maxword - 1)
    {
        len = maxword - 1;   /* 双保险：切词层已按 TOKEN_MAX-1 截断 */
    }
    memcpy(stem, word, len);
    stem[len] = '\0';
    stemword(stem);   /* 原地改写：转小写 + 词干提取 */

    if (skip && stoplist_contains(skip, stem))
    {
        (*filtered)++;
        return 0;
    }
    if (index_add_position(index, stem, doc_id, pos) != 1)
    {
        (*dropped)++;   /* 同一 (词干, doc_id, pos) 重复或内存不足 */
    }
    return 0;
}

/* 切一篇文档（文件流）并把词条插进 index，返回读到的词数；dump 写失败返回 -1。 */
static long tokenize_document(inverted_index *index, FILE *in, int doc_id,
                              const stoplist *skip, FILE *dump,
                              long *filtered, long *dropped)
{
    char word[TOKEN_MAX];
    size_t n = 0;
    int pos = 0;
    int c;
    long words = 0;

    /* 这一篇文档出现在语料里就记进文档表（df/N 的 N 要算上所有文档，
     * 哪怕它的词全被判成了停用词）。 */
    if (index_add_doc(&index->docs, doc_id) < 0)
    {
        fprintf(stderr, "out of memory while recording doc_id %d\n", doc_id);
        return -1;
    }

    /* 逐字符读：跨行的词自然会被接上（行尾换行只是分隔符），也不需要假设一行的长度上限。 */
    while ((c = fgetc(in)) != EOF)
    {
        if (isalnum((unsigned char)c))
        {
            if (n + 1 < TOKEN_MAX)
            {
                word[n++] = (char)tolower((unsigned char)c);
            }
        }
        else if (n > 0)
        {
            word[n] = '\0';
            if (store_word(index, word, doc_id, pos, skip, dump, filtered, dropped) < 0)
            {
                return -1;
            }
            pos++;
            words++;
            n = 0;
        }
    }

    if (n > 0)   /* 文件末尾没有分隔符：把最后一个词补出去 */
    {
        word[n] = '\0';
        if (store_word(index, word, doc_id, pos, skip, dump, filtered, dropped) < 0)
        {
            return -1;
        }
        words++;
    }
    return words;
}

/* 扫一遍全部文档。skip 非空时跳过停用词（pass 2）；dump 非空时写出三元组（pass 1）。 */
static int run_pass(inverted_index *index, const doc_source *docs, int ndocs,
                    const stoplist *skip, const char *label, FILE *dump)
{
    long words = 0, filtered = 0, dropped = 0;

    for (int i = 0; i < ndocs; i++)
    {
        FILE *in;
        if (docs[i].spool)
        {
            if (fseek(docs[i].spool, 0, SEEK_SET) != 0)
            {
                fprintf(stderr, "Error rewinding stdin copy\n");
                return -1;
            }
            in = docs[i].spool;
        }
        else
        {
            in = fopen(docs[i].path, "r");
            if (!in)
            {
                fprintf(stderr, "Error opening %s\n", docs[i].path);
                return -1;
            }
        }

        long got = tokenize_document(index, in, i, skip, dump, &filtered, &dropped);
        int read_failed = ferror(in);
        if (!docs[i].spool)
        {
            fclose(in);
        }
        if (got < 0 || read_failed)
        {
            fprintf(stderr, "Error reading %s\n", docs[i].spool ? "(stdin)" : docs[i].path);
            return -1;
        }
        words += got;
    }

    fprintf(stderr, "%s: %d documents, %ld words, %ld stop-word words skipped,"
                    " %ld positions dropped\n",
            label, ndocs, words, filtered, dropped);
    return 0;
}

/* ---------------- main ---------------- */

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [--theta=<value>] [--count-only] [--dump-tokens[=<file>]] <file>...\n",
            program);
    fprintf(stderr, "  每个文件 = 一篇文档，doc_id 按命令行顺序 0,1,2...；用 - 表示从 stdin 读一篇\n");
    fprintf(stderr, "  --theta=<value>  df/N 超过它就判为停用词（默认 STOPWORD_THETA = %.3f）\n",
            (double)STOPWORD_THETA);
    fprintf(stderr, "  --count-only     只跑 Part 1（词频统计 + stoplist.txt），不写 index.bin\n");
    fprintf(stderr, "  --dump-tokens[=<file>]  顺便把切词得到的三元组写出来（默认 %s），供测试对拍\n",
            DUMP_FILE);
}

int main(int argc, char *argv[])
{
    double theta = STOPWORD_THETA;
    int count_only = 0;
    const char *dump_path = NULL;
    const char *files[argc > 0 ? argc : 1];
    int nfiles = 0;

    int status = 1;
    inverted_index *index = NULL;
    term_stat *stats = NULL;
    stoplist sl = {NULL, 0, 0};
    doc_source *docs = NULL;
    FILE *dump = NULL;
    long v = 0;
    int stop_count = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--count-only") == 0)
        {
            count_only = 1;
        }
        else if (strncmp(argv[i], "--theta=", 8) == 0)
        {
            char *end = NULL;
            theta = strtod(argv[i] + 8, &end);
            if (!end || *end != '\0' || theta < 0.0 || theta > 1.0)
            {
                fprintf(stderr, "invalid --theta value: %s\n", argv[i] + 8);
                usage(argv[0]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--dump-tokens") == 0)
        {
            dump_path = DUMP_FILE;
        }
        else if (strncmp(argv[i], "--dump-tokens=", 14) == 0)
        {
            dump_path = argv[i] + 14;
        }
        else if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
        else
        {
            files[nfiles++] = argv[i];
        }
    }

    if (nfiles == 0)
    {
        fprintf(stderr, "no input documents given\n");
        usage(argv[0]);
        return 2;
    }

    docs = malloc((size_t)nfiles * sizeof(doc_source));
    if (!docs)
    {
        fprintf(stderr, "out of memory\n");
        goto cleanup;
    }
    for (int i = 0; i < nfiles; i++)
    {
        docs[i].path = files[i];
        docs[i].spool = NULL;
    }
    for (int i = 0; i < nfiles; i++)
    {
        if (strcmp(files[i], "-") == 0 && spool_stdin(&docs[i]) < 0)
        {
            goto cleanup;
        }
    }

    if (dump_path)
    {
        dump = fopen(dump_path, "w");
        if (!dump)
        {
            fprintf(stderr, "Error creating %s\n", dump_path);
            goto cleanup;
        }
    }

    /* ---- pass 1：统计（Part 1） ---- */
    index = index_create();
    if (!index)
    {
        fprintf(stderr, "out of memory\n");
        goto cleanup;
    }
    if (run_pass(index, docs, nfiles, NULL, "pass 1 (statistics)", dump) < 0)
    {
        goto cleanup;
    }
    if (dump)
    {
        int failed = (fclose(dump) != 0);
        dump = NULL;
        if (failed)
        {
            fprintf(stderr, "Error writing %s\n", dump_path);
            goto cleanup;
        }
    }

    stats = collect_stats(index, &v);
    if (!stats)
    {
        fprintf(stderr, "out of memory while collecting statistics\n");
        goto cleanup;
    }

    stop_count = stoplist_build(index, theta, &sl);
    if (stop_count < 0)
    {
        goto cleanup;
    }

    long positions = 0;
    for (long i = 0; i < v; i++)
    {
        positions += stats[i].tf;
    }
    fprintf(stderr, "part 1: N=%d documents, V=%ld stems, %ld positions,"
                    " %d stop words (df/N > %.3f)\n",
            index->docs.count, v, positions, stop_count, theta);
    qsort(stats, (size_t)v, sizeof(term_stat), cmp_stat_df_desc);   /* 报告与 stoplist.txt 都按 df 降序 */

    if (stoplist_save(STOPLIST_FILE, stats, v, &sl, theta, index->docs.count) < 0)
    {
        goto cleanup;
    }

    if (count_only)
    {
        wc_report(index, stats, v, &sl, theta);
        status = 0;
        goto cleanup;
    }

    /* ---- pass 2：建索引（Part 2） ---- */
    index_free(index);   /* pass 1 的索引只为统计，整份丢掉，避免两份索引同时驻留 */
    index = index_create();
    if (!index)
    {
        fprintf(stderr, "out of memory\n");
        goto cleanup;
    }
    if (run_pass(index, docs, nfiles, &sl, "pass 2 (index generation)", NULL) < 0)
    {
        goto cleanup;
    }

    /* 先写 .tmp 再 rename：写到一半失败（磁盘满、被中断）不会留下一个半截的 index.bin
     * 让 query 读到"看起来能用"的坏索引。 */
    const char *tmp_path = INDEX_FILE ".tmp";
    FILE *out = fopen(tmp_path, "wb");
    if (!out)
    {
        fprintf(stderr, "Error creating %s\n", tmp_path);
        goto cleanup;
    }

    long terms = index_save(index, out);
    int close_failed = (fclose(out) != 0);
    if (terms < 0 || close_failed)
    {
        fprintf(stderr, "Error writing %s\n", tmp_path);
        remove(tmp_path);
        goto cleanup;
    }
    if (rename(tmp_path, INDEX_FILE) != 0)
    {
        fprintf(stderr, "Error renaming %s to %s\n", tmp_path, INDEX_FILE);
        remove(tmp_path);
        goto cleanup;
    }

    fprintf(stderr, "wrote %ld terms to %s and %d stop words to %s\n",
            terms, INDEX_FILE, stop_count, STOPLIST_FILE);
    status = 0;

cleanup:
    if (dump)
    {
        fclose(dump);
    }
    if (docs)
    {
        for (int i = 0; i < nfiles; i++)
        {
            if (docs[i].spool)
            {
                fclose(docs[i].spool);
            }
        }
        free(docs);
    }
    free(stats);
    stoplist_free(&sl);
    index_free(index);
    return status;
}
