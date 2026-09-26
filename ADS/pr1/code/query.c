/* pr1 Part 3：查询程序。
 *
 * 已实现：加载 index_gen 写出的 index.bin -> 查询词词干化 -> 查倒排索引 ->
 *         输出 df/N、tf 和按 (doc_id, pos) 升序的命中位置。
 * 未实现（见文末 TODO）：短语查询、查询阈值 τ。
 *
 * 用法：
 *   ./query                 # 读 test.txt（按空白切词），结果写 output.txt
 *   ./query run zebra       # 直接查命令行给出的词，结果同样写 output.txt
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o query query.c index_io.c stem.c -lm
 */
#include <stdio.h>
#include <string.h>
#include "index.h"
#include "stem.h"

/* 查询单个词。参数 word 不会被改写（在本地缓冲里做词干化）。 */
void query_word(inverted_index *index, const char *word, FILE *output)
{
    char stem[maxword];
    snprintf(stem, sizeof(stem), "%s", word);
    stemword(stem);

    Posting *posting = index_find(index, stem);
    int total_docs = index->docs.count;

    if (!posting)
    {
        fprintf(output, "Not found: %s\n", stem);
        return;
    }

    int df = posting_df(posting);
    fprintf(output, "Found: %s  (df=%d/%d", stem, df, total_docs);
    if (total_docs > 0)
    {
        fprintf(output, " = %.3f", (double)df / (double)total_docs);
    }
    fprintf(output, ", tf=%d)\n", posting->occurrences);

    for (const Position *pos = posting->positions; pos; pos = pos->next)
    {
        fprintf(output, "Doc ID: %d, Position: %d\n", pos->doc_id, pos->pos);
    }
}

int main(int argc, char *argv[])
{
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

    FILE *output = fopen("output.txt", "w");
    if (!output)
    {
        fprintf(stderr, "Error opening output.txt\n");
        index_free(index);
        return 1;
    }

    long queried = 0;
    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            query_word(index, argv[i], output);
            queried++;
        }
    }
    else
    {
        FILE *input = fopen("test.txt", "r");
        if (!input)
        {
            fprintf(stderr, "Error opening test.txt (or pass query words as arguments)\n");
            fclose(output);
            index_free(index);
            return 1;
        }
        char word[maxword];
        while (fscanf(input, "%255s", word) == 1)
        {
            query_word(index, word, output);
            queried++;
        }
        fclose(input);
    }

    if (fclose(output) != 0)
    {
        fprintf(stderr, "Error writing output.txt\n");
        index_free(index);
        return 1;
    }

    fprintf(stderr, "queried %ld word(s) against %d document(s), results in output.txt\n",
            queried, index->docs.count);
    index_free(index);
    return 0;
}

/* TODO（Part 3 / Part 4）：
 *   1. 短语查询：题面写的是 "a user-specified word (or phrase)"。位置链已经是升序的，
 *      可以对各词取交集后逐文档检查 pos 连续（README.md 第 10 节）；
 *   2. 查询阈值 τ：df/N 过高的词按硬阈值（报 too common）或软阈值（top-K + tf-idf 排序）处理。
 */
