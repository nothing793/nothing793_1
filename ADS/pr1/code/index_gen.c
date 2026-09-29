/* pr1 Part 1 + Part 2：统计建索引程序。
 *
 * 一个程序做两件事（对应题面的第 1 问与第 2 问）：
 *
 *   Part 1（词频统计 + 停用词识别）—— 原来打算放在 wordcount.c，现已并入本文件：
 *     pass 1：读 file.txt 的三元组，把整份语料插进内存倒排索引（顺手得到 cf / df）
 *             逐词条算 df / N，> θ 的判为停用词（noisy words），落盘 stoplist.txt。
 *     θ 的来源：编译期宏 STOPWORD_THETA（默认 0.5，可用 -DSTOPWORD_THETA=0.4 覆盖），
 *              运行时还可以用 --theta=0.4 覆盖宏。
 *
 *   Part 2（带词干化的倒排索引，不含停用词）：
 *     pass 2：再读一遍 file.txt，跳过停用词，重建索引，写 index.bin。
 *
 * 为什么要扫两遍：df 是"整份语料"的统计量，N 只有扫完才知道；而题面明写索引里
 * 不能包含 Part 1 认定的停用词，所以必须先统计完再建索引。两遍都用同一个
 * read_triples() / index_add_position() 入口，因此统计口径与建索引口径不可能不一致。
 * （代价：建索引时间约翻倍；真实语料 88 万条三元组实测约 2.6 s，可接受。Bonus 规模
 * 的 SPIMI 路径也必须先得到 stoplist，所以两遍扫描同样是那条路要走的结构。）
 *
 * 用法：
 *   ./index_gen                      # θ = STOPWORD_THETA，写 index.bin + stoplist.txt
 *   ./index_gen --theta=0.4          # 换阈值
 *   ./index_gen --count-only         # 只做 Part 1：打印词频报告 + 写 stoplist.txt，不建索引
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o index_gen index_gen.c index_io.c stem.c -lm
 */
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

/* ---------------- 读三元组（两遍扫描共用） ---------------- */

/* 读 "<word> <doc_id> <pos>" 三元组，词干化后插入 index；skip 非空时跳过停用词。
 *   - 循环条件写成 fscanf(...) == 3，而不是 while (!feof(...))：后者在文件末尾
 *     还会拿上一次的（首次是未初始化的）doc_id / pos 再插一条；
 *   - doc_id / pos 为负的行跳过并计数；
 *   - 格式不符的行会让 fscanf 提前返回，其后内容不再读入（要容忍脏输入得按行读）。
 * 返回入库的位置条目数。 */
static long read_triples(inverted_index *index, FILE *file, const stoplist *skip, const char *label)
{
    char word[maxword];
    int doc_id, pos;
    long added = 0, duplicates = 0, invalid = 0, filtered = 0;

    while (fscanf(file, "%255s %d %d", word, &doc_id, &pos) == 3)
    {
        if (doc_id < 0 || pos < 0)
        {
            invalid++;
            continue;
        }
        stemword(word);   /* 原地改写：转小写 + 词干提取 */

        if (index_add_doc(&index->docs, doc_id) < 0)
        {
            fprintf(stderr, "out of memory while recording doc_id %d\n", doc_id);
            break;
        }
        if (skip && stoplist_contains(skip, word))
        {
            filtered++;
            continue;
        }
        if (index_add_position(index, word, doc_id, pos))
        {
            added++;
        }
        else
        {
            duplicates++;
        }
    }

    fprintf(stderr, "%s: %ld positions, %ld stop-word positions skipped,"
                    " %ld duplicates skipped, %ld invalid lines skipped\n",
            label, added, filtered, duplicates, invalid);
    return added;
}

/* 打开当前工作目录下的 file.txt（两遍扫描各开一次） */
static FILE *open_triples(void)
{
    FILE *file = fopen("file.txt", "r");
    if (!file)
    {
        fprintf(stderr, "Error opening file.txt\n");
    }
    return file;
}

/* ---------------- main ---------------- */

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [--theta=<value>] [--count-only]\n", program);
    fprintf(stderr, "  --theta=<value>  df/N 超过它就判为停用词（默认 STOPWORD_THETA = %.3f）\n",
            (double)STOPWORD_THETA);
    fprintf(stderr, "  --count-only     只跑 Part 1（词频统计 + stoplist.txt），不写 index.bin\n");
}

int main(int argc, char *argv[])
{
    double theta = STOPWORD_THETA;
    int count_only = 0;

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
        else
        {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    /* ---- pass 1：统计（Part 1） ---- */
    inverted_index *index = index_create();
    if (!index)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    FILE *file = open_triples();
    if (!file)
    {
        index_free(index);
        return 1;
    }
    read_triples(index, file, NULL, "pass 1 (statistics)");
    fclose(file);

    long v = 0;
    term_stat *stats = collect_stats(index, &v);
    if (!stats)
    {
        fprintf(stderr, "out of memory while collecting statistics\n");
        index_free(index);
        return 1;
    }

    stoplist sl = {NULL, 0, 0};
    int stop_count = stoplist_build(index, theta, &sl);
    if (stop_count < 0)
    {
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }

    long positions = 0;
    for (long i = 0; i < v; i++)
    {
        positions += stats[i].tf;
    }
    fprintf(stderr, "part 1: N=%d documents, V=%ld stems, %ld positions, %d stop words (df/N > %.3f)\n",
            index->docs.count, v, positions, stop_count, theta);
    qsort(stats, (size_t)v, sizeof(term_stat), cmp_stat_df_desc);   /* 报告与 stoplist.txt 都按 df 降序 */

    if (stoplist_save(STOPLIST_FILE, stats, v, &sl, theta, index->docs.count) < 0)
    {
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }

    if (count_only)
    {
        wc_report(index, stats, v, &sl, theta);
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 0;
    }

    /* ---- pass 2：建索引（Part 2） ---- */
    index_free(index);   /* pass 1 的索引只为统计，整份丢掉，避免两份索引同时驻留 */
    index = index_create();
    if (!index)
    {
        fprintf(stderr, "out of memory\n");
        free(stats);
        stoplist_free(&sl);
        return 1;
    }
    file = open_triples();
    if (!file)
    {
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }
    read_triples(index, file, &sl, "pass 2 (index generation)");
    fclose(file);

    /* 先写 .tmp 再 rename：写到一半失败（磁盘满、被中断）不会留下一个半截的 index.bin
     * 让 query 读到"看起来能用"的坏索引。 */
    const char *tmp_path = INDEX_FILE ".tmp";
    FILE *out = fopen(tmp_path, "wb");
    if (!out)
    {
        fprintf(stderr, "Error creating %s\n", tmp_path);
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }

    long terms = index_save(index, out);
    int close_failed = (fclose(out) != 0);
    if (terms < 0 || close_failed)
    {
        fprintf(stderr, "Error writing %s\n", tmp_path);
        remove(tmp_path);
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }
    if (rename(tmp_path, INDEX_FILE) != 0)
    {
        fprintf(stderr, "Error renaming %s to %s\n", tmp_path, INDEX_FILE);
        remove(tmp_path);
        free(stats);
        stoplist_free(&sl);
        index_free(index);
        return 1;
    }

    fprintf(stderr, "wrote %ld terms to %s and %d stop words to %s\n",
            terms, INDEX_FILE, stop_count, STOPLIST_FILE);

    free(stats);
    stoplist_free(&sl);
    index_free(index);
    return 0;
}
