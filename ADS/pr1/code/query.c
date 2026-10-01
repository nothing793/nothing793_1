/* pr1 Part 3 + Part 4：查询程序。
 *
 * 做三件事：
 *   1. 加载 index_gen 写出的 index.bin（index_load 会校验格式，损坏就拒绝加载）；
 *   2. 接受用户给出的词（或短语），返回包含它的文档 ID：
 *        单词查询    打印 df/N、tf 和全部命中位置；
 *        多词 AND    对多个查询单元取交集，打印共同命中的文档 ID；
 *        短语查询    先取交集，再按位置链验证位置连续；
 *   3. 查询阈值 τ：df/N > τ 的词判为 too common，走硬阈值（报出命中文档数，但不打印
 *      完整文档列表）。这是题面第 4 问要测的东西：阈值如何改变结果。
 *
 * 用法：
 *   ./query run                     # 单词
 *   ./query run zebra               # 多个参数 = AND（交集）
 *   ./query "to be or not to be"    # 单个参数含空白 = 短语（位置必须连续）
 *   ./query --tau=0.3 the           # 开阈值
 *   ./query                         # 不给参数则读 test.txt，每行一个查询
 *
 * test.txt 的行格式：单个词 = 单词查询；多个词 = AND；`phrase: a b c` = 短语查询。
 *
 * 结果都写进 output.txt（每次运行覆盖），每个查询一段、以 "# query" 开头。
 * stoplist.txt（index_gen 生成）存在时，用来解释 "Not found" 是不是停用词；
 * 它只影响提示文字，不影响查询结果 —— 索引里有什么完全由 index.bin 决定。
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o query query.c stem.c -lm
 *       （索引模块在 index.h 里，本文件 #include 它即可，没有单独的 index_io.c）
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"
#include "stem.h"

#define MAX_UNITS 32          /* 一次查询最多几个单元（每个单元 = 一个词或一个短语） */
#define MAX_PHRASE 16         /* 一个短语最多几个词 */
#define STOPLIST_FILE "stoplist.txt"

/* ---------------- stoplist.txt（只用于解释 "Not found"，可选） ---------------- */

typedef struct stop_word
{
    char stem[maxword];
    int df;
    double ratio;
} stop_word;

/* 读 stoplist.txt；文件不存在或读不成时返回 NULL（查询照常进行） */
static stop_word *stoplist_load(int *out_count, double *out_theta)
{
    *out_count = 0;
    *out_theta = 0.0;

    FILE *file = fopen(STOPLIST_FILE, "r");
    if (!file)
    {
        return NULL;
    }

    int capacity = 64, count = 0;
    stop_word *list = malloc((size_t)capacity * sizeof(stop_word));
    char line[512];

    while (list && fgets(line, sizeof(line), file))
    {
        if (line[0] == '#')
        {
            sscanf(line, "# stoplist: theta=%lf", out_theta);
            continue;
        }
        char term[maxword];
        int cf = 0, df = 0;
        double ratio = 0.0;
        if (sscanf(line, "%255s %d %d %lf", term, &cf, &df, &ratio) == 4)
        {
            if (count == capacity)
            {
                capacity *= 2;
                stop_word *grown = realloc(list, (size_t)capacity * sizeof(stop_word));
                if (!grown)
                {
                    break;
                }
                list = grown;
            }
            snprintf(list[count].stem, sizeof(list[count].stem), "%s", term);
            list[count].df = df;
            list[count].ratio = ratio;
            count++;
        }
    }
    fclose(file);
    *out_count = count;
    return list;
}

static const stop_word *stoplist_find(const stop_word *list, int count, const char *stem)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(list[i].stem, stem) == 0)
        {
            return &list[i];
        }
    }
    return NULL;
}

/* ---------------- 查询词 ---------------- */

typedef struct query_term
{
    char stem[maxword];     /* 词干（索引里的 key）；不改写调用方的字符串 */
    Posting *posting;       /* NULL = 索引里没有这个词 */
    int df, tf;
} query_term;

static void term_init(query_term *qt, const char *raw)
{
    snprintf(qt->stem, sizeof(qt->stem), "%s", raw);
    stemword(qt->stem);
    qt->posting = NULL;
    qt->df = qt->tf = 0;
}

/* 查索引 + 处理阈值 τ，并把这个词的情况写进 out。
 * 返回 1 = 可用于求交，0 = 不可用（没进索引，或被 τ 拦下）。 */
static int term_resolve(inverted_index *index, query_term *qt, double tau, FILE *out,
                        const stop_word *stoplist, int stop_count, double theta)
{
    qt->posting = index_find(index, qt->stem);
    int total_docs = index->docs.count;

    if (!qt->posting)
    {
        const stop_word *hit = stoplist_find(stoplist, stop_count, qt->stem);
        if (hit)
        {
            fprintf(out, "Not found: %s  (stop word: df=%d/%d = %.4f > theta=%.4f,"
                         " removed from the index in part 1)\n",
                    qt->stem, hit->df, total_docs, hit->ratio, theta);
        }
        else
        {
            fprintf(out, "Not found: %s\n", qt->stem);
        }
        return 0;
    }

    qt->df = posting_df(qt->posting);
    qt->tf = qt->posting->occurrences;

    if (tau >= 0.0 && total_docs > 0 && (double)qt->df / (double)total_docs > tau)
    {
        fprintf(out, "Too common: %s  (df=%d/%d = %.3f, tf=%d, tau=%.3f ->"
                     " %d documents, result list suppressed)\n",
                qt->stem, qt->df, total_docs, (double)qt->df / (double)total_docs,
                qt->tf, tau, qt->df);
        return 0;
    }

    fprintf(out, "Found: %s  (df=%d/%d", qt->stem, qt->df, total_docs);
    if (total_docs > 0)
    {
        fprintf(out, " = %.3f", (double)qt->df / (double)total_docs);
    }
    fprintf(out, ", tf=%d)\n", qt->tf);
    return 1;
}

/* ---------------- 位置链 -> 文档号 / 位置数组 ---------------- */

/* 出现过的文档号（升序去重）写进 docs，返回个数 */
static int posting_docs(const Posting *posting, int *docs)
{
    int n = 0, last = -1;
    for (const Position *p = posting->positions; p; p = p->next)
    {
        if (p->doc_id != last)
        {
            docs[n++] = p->doc_id;
            last = p->doc_id;
        }
    }
    return n;
}

/* 文档 doc 内的位置（升序）写进 positions，返回个数 */
static int posting_positions(const Posting *posting, int doc, int *positions)
{
    int n = 0;
    for (const Position *p = posting->positions; p; p = p->next)
    {
        if (p->doc_id == doc)
        {
            positions[n++] = p->pos;
        }
        else if (p->doc_id > doc)
        {
            break;
        }
    }
    return n;
}

static int contains_int(const int *sorted, int n, int value)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (sorted[mid] == value)
        {
            return 1;
        }
        if (sorted[mid] < value)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return 0;
}

/* k 个升序数组的交集写进 out，返回个数；lists[0] 当锚点，每轮一定前进，不会死循环 */
static int intersect_docs(int *const *lists, const int *counts, int k, int *out)
{
    if (k == 0)
    {
        return 0;
    }
    int pos[MAX_UNITS];
    int n = 0;
    for (int i = 0; i < k; i++)
    {
        pos[i] = 0;
    }

    while (pos[0] < counts[0])
    {
        int candidate = lists[0][pos[0]];
        int all = 1;
        for (int i = 1; i < k; i++)
        {
            while (pos[i] < counts[i] && lists[i][pos[i]] < candidate)
            {
                pos[i]++;
            }
            if (pos[i] >= counts[i])
            {
                return n;
            }
            if (lists[i][pos[i]] != candidate)
            {
                all = 0;
                while (pos[0] < counts[0] && lists[0][pos[0]] < lists[i][pos[i]])
                {
                    pos[0]++;   /* 锚点跳到下一个可能命中的位置 */
                }
                break;
            }
        }
        if (all)
        {
            out[n++] = candidate;
            pos[0]++;
        }
    }
    return n;
}

/* ---------------- 单词 / AND / 短语 ---------------- */

/* 求一组词的文档交集。返回 -1 = 内存不足；否则返回共同命中文档数。
 * 任何一个词不可用时（没进索引 / 被 τ 拦下）结果就是空集。 */
static int run_word_query(query_term *terms, int n, int *docs)
{
    int *lists[MAX_UNITS];
    int counts[MAX_UNITS];
    int k = 0;

    for (int i = 0; i < n; i++)
    {
        if (!terms[i].posting)
        {
            continue;
        }
        int *buf = malloc((size_t)terms[i].df * sizeof(int));
        if (!buf)
        {
            for (int j = 0; j < k; j++)
            {
                free(lists[j]);
            }
            return -1;
        }
        counts[k] = posting_docs(terms[i].posting, buf);
        lists[k++] = buf;
    }

    int found = (k == n) ? intersect_docs(lists, counts, k, docs) : 0;
    for (int j = 0; j < k; j++)
    {
        free(lists[j]);
    }
    return found;
}

/* 短语查询：先对各词的文档取交集，再逐候选文档验证位置连续（pos, pos+1, pos+2...）。
 * 返回命中的起始位置个数；start_docs / starts 平行记录（升序）。
 * 返回 -1 = 内存不足。 */
static int run_phrase_query(query_term *terms, int n, int *docs, int *start_docs, int *starts)
{
    /* 用 tf 最小的词当锚点：候选起始位置最少，验证代价最低。会改动 terms 的顺序，
     * 所以调用方若要按原顺序打印，得在那之前完成。 */
    for (int i = 1; i < n; i++)
    {
        for (int j = i; j > 0 && terms[j].tf < terms[j - 1].tf; j--)
        {
            query_term tmp = terms[j];
            terms[j] = terms[j - 1];
            terms[j - 1] = tmp;
        }
    }

    for (int i = 0; i < n; i++)
    {
        if (!terms[i].posting)
        {
            return 0;   /* 有词不可用 -> 短语不可能命中 */
        }
    }

    int *lists[MAX_UNITS];
    int counts[MAX_UNITS];
    int *positions[MAX_UNITS];
    int doc_limit = terms[0].df;
    for (int i = 0; i < n; i++)
    {
        if (terms[i].df < doc_limit)
        {
            doc_limit = terms[i].df;
        }
    }
    int *candidate_docs = malloc((size_t)(doc_limit > 0 ? doc_limit : 1) * sizeof(int));

    int allocated = 0;
    int failed = (candidate_docs == NULL);
    for (int i = 0; i < n && !failed; i++)
    {
        lists[i] = malloc((size_t)terms[i].df * sizeof(int));
        positions[i] = malloc((size_t)terms[i].tf * sizeof(int));
        if (!lists[i] || !positions[i])
        {
            allocated = i + 1;
            failed = 1;
            break;
        }
        counts[i] = posting_docs(terms[i].posting, lists[i]);
        allocated = i + 1;
    }

    int found = -1;
    if (!failed)
    {
        int candidate_count = intersect_docs(lists, counts, n, candidate_docs);
        found = 0;
        for (int c = 0; c < candidate_count; c++)
        {
            int doc = candidate_docs[c];
            int counts_in_doc[MAX_UNITS];
            for (int i = 0; i < n; i++)
            {
                counts_in_doc[i] = posting_positions(terms[i].posting, doc, positions[i]);
            }
            for (int a = 0; a < counts_in_doc[0]; a++)
            {
                int start = positions[0][a];
                int ok = 1;
                for (int i = 1; i < n; i++)
                {
                    if (!contains_int(positions[i], counts_in_doc[i], start + i))
                    {
                        ok = 0;
                        break;
                    }
                }
                if (ok)
                {
                    start_docs[found] = doc;
                    starts[found] = start;
                    found++;
                }
            }
        }

        /* 短语命中过的文档（升序去重） */
        int doc_n = 0;
        for (int i = 0; i < found; i++)
        {
            if (doc_n == 0 || docs[doc_n - 1] != start_docs[i])
            {
                docs[doc_n++] = start_docs[i];
            }
        }
    }

    for (int i = 0; i < allocated; i++)
    {
        free(lists[i]);
        free(positions[i]);
    }
    free(candidate_docs);
    return found;
}

/* ---------------- 一条查询 ---------------- */

/* 把一个空白分隔的单元切成若干查询词，返回词数；多于 max 时多余部分忽略并返回 -1 */
static int split_terms(const char *text, query_term *terms, int max)
{
    char buf[MAX_PHRASE * maxword];
    snprintf(buf, sizeof(buf), "%s", text);

    int n = 0;
    for (char *tok = strtok(buf, " \t"); tok; tok = strtok(NULL, " \t"))
    {
        if (n >= max)
        {
            return -1;
        }
        term_init(&terms[n++], tok);
    }
    return n;
}

/* 运行一条查询（unit_count 个单元）并把结果写进 out，返回命中文档数（-1 = 出错） */
static int process_query(inverted_index *index, char *const *units, int unit_count, FILE *out,
                         const stop_word *stoplist, int stop_count, double theta, double tau,
                         int query_no)
{
    fprintf(out, "# query %d:", query_no);
    for (int i = 0; i < unit_count; i++)
    {
        fprintf(out, " %s", units[i]);
    }
    fprintf(out, "\n");

    int unit_doc_counts[MAX_UNITS];
    int *unit_doc_bufs[MAX_UNITS];
    for (int i = 0; i < unit_count; i++)
    {
        unit_doc_bufs[i] = NULL;
    }

    /* 一个单元 = 一个词（无空白）或一个短语（含空白） */
    int phrase_unit_count = 0;
    for (int u = 0; u < unit_count; u++)
    {
        if (strpbrk(units[u], " \t") != NULL)
        {
            phrase_unit_count++;
        }
    }

    int single_word = (unit_count == 1 && phrase_unit_count == 0);
    int result_docs = -1;

    for (int u = 0; u < unit_count && result_docs != -2; u++)
    {
        query_term terms[MAX_PHRASE];
        int n = split_terms(units[u], terms, MAX_PHRASE);
        if (n < 0)
        {
            fprintf(out, "Error: too many words in one query unit (max %d)\n", MAX_PHRASE);
            result_docs = -2;
            break;
        }
        if (n == 0)
        {
            fprintf(out, "(empty query unit, skipped)\n");
            unit_doc_counts[u] = 0;
            continue;
        }

        /* 先解析每个词（打印 Found / Not found / Too common） */
        int usable = 1;
        for (int i = 0; i < n; i++)
        {
            if (!term_resolve(index, &terms[i], tau, out, stoplist, stop_count, theta))
            {
                usable = 0;
            }
        }

        int is_phrase = (n > 1) || (strpbrk(units[u], " \t") != NULL);
        int doc_count = 0;
        int *docs = malloc((size_t)(terms[0].tf > 0 ? terms[0].tf : 1) * sizeof(int));
        if (!docs)
        {
            result_docs = -2;
            break;
        }

        if (single_word)
        {
            /* 单词查询：沿用原来的输出格式，逐条打印 (doc, pos)。
             * 词不存在或被 τ 拦下时不打印列表（term_resolve 已经说明了原因）。 */
            if (usable)
            {
                for (const Position *p = terms[0].posting->positions; p; p = p->next)
                {
                    fprintf(out, "Doc ID: %d, Position: %d\n", p->doc_id, p->pos);
                }
                doc_count = terms[0].df;
            }
        }
        else if (is_phrase)
        {
            if (!usable)
            {
                /* 短语里含停用词（索引里没有）或词被 τ 拦下 -> 明确报 0 命中 */
                fprintf(out, "Phrase [%s]: 0 match(es) in 0 document(s)\n", units[u]);
                free(docs);
                docs = NULL;
                unit_doc_bufs[u] = malloc(sizeof(int));
                unit_doc_counts[u] = 0;
                if (!unit_doc_bufs[u])
                {
                    result_docs = -2;
                }
                continue;
            }
            int match_limit = (terms[0].tf > 0) ? terms[0].tf : 1;
            for (int i = 1; i < n; i++)
            {
                if (terms[i].tf < terms[0].tf)
                {
                    match_limit = terms[i].tf > 0 ? terms[i].tf : 1;
                }
            }
            int *start_docs = malloc((size_t)match_limit * sizeof(int));
            int *starts = malloc((size_t)match_limit * sizeof(int));
            if (!start_docs || !starts)
            {
                free(start_docs);
                free(starts);
                free(docs);
                result_docs = -2;
                break;
            }
            int matches = run_phrase_query(terms, n, docs, start_docs, starts);
            if (matches < 0)
            {
                free(start_docs);
                free(starts);
                free(docs);
                result_docs = -2;
                break;
            }
            doc_count = 0;
            for (int i = 0; i < matches; i++)
            {
                if (i == 0 || start_docs[i] != start_docs[i - 1])
                {
                    doc_count++;
                }
            }
            fprintf(out, "Phrase [%s]: %d match(es) in %d document(s)\n", units[u], matches, doc_count);
            int i = 0;
            while (i < matches)
            {
                int doc = start_docs[i];
                fprintf(out, "Doc ID: %d, Positions:", doc);
                int j = i;
                while (j < matches && start_docs[j] == doc)
                {
                    fprintf(out, " %d", starts[j]);
                    j++;
                }
                fprintf(out, "\n");
                i = j;
            }
            free(start_docs);
            free(starts);
        }
        else
        {
            doc_count = usable ? run_word_query(terms, n, docs) : 0;
            if (doc_count < 0)
            {
                free(docs);
                result_docs = -2;
                break;
            }
            if (n == 1 && !usable)
            {
                doc_count = 0;   /* Not found / Too common，term_resolve 已经说明原因 */
            }
        }

        /* 保存本单元命中的文档，供多单元求交 */
        int *keep = malloc((size_t)(doc_count > 0 ? doc_count : 1) * sizeof(int));
        if (!keep)
        {
            free(docs);
            result_docs = -2;
            break;
        }
        memcpy(keep, docs, (size_t)doc_count * sizeof(int));
        unit_doc_bufs[u] = keep;
        unit_doc_counts[u] = doc_count;
        free(docs);
    }

    if (result_docs == -2)
    {
        for (int i = 0; i < unit_count; i++)
        {
            free(unit_doc_bufs[i]);
        }
        return -1;
    }

    int *final_docs = malloc((size_t)(unit_doc_counts[0] > 0 ? unit_doc_counts[0] : 1) * sizeof(int));
    if (!final_docs)
    {
        for (int i = 0; i < unit_count; i++)
        {
            free(unit_doc_bufs[i]);
        }
        return -1;
    }

    int found = intersect_docs((int *const *)unit_doc_bufs, unit_doc_counts, unit_count, final_docs);
    if (!single_word && unit_count > 1)   /* 单个单元的汇总已经在上面那行里给过了 */
    {
        fprintf(out, "AND [");
        for (int i = 0; i < unit_count; i++)
        {
            fprintf(out, "%s%s", i ? " " : "", units[i]);
        }
        fprintf(out, "]: %d document(s)\n", found);
        for (int i = 0; i < found; i++)
        {
            fprintf(out, "Doc ID: %d\n", final_docs[i]);
        }
    }

    for (int i = 0; i < unit_count; i++)
    {
        free(unit_doc_bufs[i]);
    }
    free(final_docs);
    return single_word ? unit_doc_counts[0] : found;
}

/* ---------------- main ---------------- */

static char *trim(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && isspace((unsigned char)line[n - 1]))
    {
        line[--n] = '\0';
    }
    char *start = line;
    while (*start && isspace((unsigned char)*start))
    {
        start++;
    }
    return start;
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [--tau=<value>] <word|phrase>...\n", program);
    fprintf(stderr, "  --tau=<value>  df/N 超过它的词判为 too common（默认不设阈值）\n");
    fprintf(stderr, "  多个参数 = AND；单个参数含空白 = 短语；不给参数则读 test.txt\n");
}

int main(int argc, char *argv[])
{
    double tau = -1.0;   /* < 0 表示不设阈值 */
    char *units[MAX_UNITS];
    int unit_count = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--tau=", 6) == 0)
        {
            char *end = NULL;
            tau = strtod(argv[i] + 6, &end);
            if (!end || *end != '\0' || tau < 0.0 || tau > 1.0)
            {
                fprintf(stderr, "invalid --tau value: %s\n", argv[i] + 6);
                usage(argv[0]);
                return 2;
            }
        }
        else if (unit_count < MAX_UNITS)
        {
            units[unit_count++] = argv[i];
        }
        else
        {
            fprintf(stderr, "too many query units (max %d)\n", MAX_UNITS);
            return 2;
        }
    }

    FILE *index_file = fopen(INDEX_FILE, "rb");
    if (!index_file)
    {
        fprintf(stderr, "Error opening %s (run ./index_gen first)\n", INDEX_FILE);
        return 1;
    }
    inverted_index *index = index_load(index_file);
    fclose(index_file);
    if (!index)
    {
        fprintf(stderr, "Error: %s is not a valid index file (re-run ./index_gen)\n", INDEX_FILE);
        return 1;
    }

    int stop_count = 0;
    double theta = 0.0;
    stop_word *stoplist = stoplist_load(&stop_count, &theta);

    FILE *output = fopen("output.txt", "w");
    if (!output)
    {
        fprintf(stderr, "Error opening output.txt\n");
        free(stoplist);
        index_free(index);
        return 1;
    }

    int queries = 0;
    int failed = 0;

    if (unit_count > 0)
    {
        queries = 1;
        if (process_query(index, units, unit_count, output, stoplist, stop_count, theta, tau, 1) < 0)
        {
            failed = 1;
        }
    }
    else
    {
        FILE *input = fopen("test.txt", "r");
        if (!input)
        {
            fprintf(stderr, "Error opening test.txt (or pass query words as arguments)\n");
            fclose(output);
            free(stoplist);
            index_free(index);
            return 1;
        }
        char line[1024];
        while (fgets(line, sizeof(line), input))
        {
            char *text = trim(line);
            if (*text == '\0')
            {
                continue;
            }
            char *line_units[2];
            int n = 0;
            if (strncmp(text, "phrase:", 7) == 0)
            {
                line_units[n++] = trim(text + 7);
            }
            else if (strpbrk(text, " \t"))
            {
                line_units[n++] = text;   /* 多个词 = AND（引号在文件里不需要） */
            }
            else
            {
                line_units[n++] = text;
            }
            queries++;
            if (process_query(index, line_units, n, output, stoplist, stop_count, theta, tau, queries) < 0)
            {
                failed = 1;
                break;
            }
        }
        fclose(input);
    }

    if (fclose(output) != 0)
    {
        fprintf(stderr, "Error writing output.txt\n");
        free(stoplist);
        index_free(index);
        return 1;
    }

    fprintf(stderr, "queried %d quer%s against %d document(s)%s, results in output.txt\n",
            queries, queries == 1 ? "y" : "ies", index->docs.count,
            tau >= 0.0 ? " (tau enabled)" : "");

    free(stoplist);
    index_free(index);
    return failed ? 1 : 0;
}
