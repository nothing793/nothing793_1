/* pr1 Part 2：建索引程序。
 *
 * 做三件事：
 *   1. 读 file.txt 的 "<word> <doc_id> <pos>" 三元组，做词干化后建内存倒排索引
 *      （djb2 哈希 + 链地址法，每个词干挂一条按 (doc_id, pos) 升序的位置链）；
 *   2. 把内存索引写成二进制索引文件 index.bin（格式见 index.h 顶部注释）；
 *   3. 在 stderr 报告统计信息。
 *
 * 未实现：按 Part 1 产出的 stoplist 剔词（题面要求索引里不能有 stop words）。
 * index.bin 带版本号，等 wordcount.c 做好后重新生成一次即可，不影响格式。
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o index_gen index_gen.c index_io.c stem.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include "index.h"
#include "stem.h"

/* 循环条件写成 fscanf(...) == 3，而不是 while (!feof(...))：
 * 后者在文件结束时还会拿上一次的（首次是未初始化的）doc_id / pos 再插一条。 */
long index_build(inverted_index *index, FILE *file)
{
    char word[maxword];
    int doc_id, pos;
    long added = 0, duplicates = 0, invalid = 0;

    /* TODO（Part 1 / Part 2）：停用词过滤尚未实现。
     * wordcount 用 df/N > θ 产出 stoplist 后，这里要在建索引前把命中 stoplist 的词干跳过，
     * 题面明写 "The stop words identified in part (1) must not be included"。 */
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
        if (index_add_position(index, word, doc_id, pos))
        {
            added++;
        }
        else
        {
            duplicates++;
        }
    }

    fprintf(stderr, "indexed: %ld positions, %d documents"
                    " (%ld duplicate positions skipped, %ld invalid lines skipped)\n",
            added, index->docs.count, duplicates, invalid);
    return added;
}

int main(void)
{
    inverted_index *index = index_create();
    if (!index)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    FILE *file = fopen("file.txt", "r");
    if (!file)
    {
        fprintf(stderr, "Error opening file.txt\n");
        index_free(index);
        return 1;
    }
    index_build(index, file);
    fclose(file);

    /* 先写 .tmp 再 rename：写到一半失败（磁盘满、被中断）不会留下一个半截的 index.bin
     * 让 query 读到"看起来能用"的坏索引。 */
    const char *tmp_path = INDEX_FILE ".tmp";
    FILE *out = fopen(tmp_path, "wb");
    if (!out)
    {
        fprintf(stderr, "Error creating %s\n", tmp_path);
        index_free(index);
        return 1;
    }

    long terms = index_save(index, out);
    int close_failed = (fclose(out) != 0);
    if (terms < 0 || close_failed)
    {
        fprintf(stderr, "Error writing %s\n", tmp_path);
        remove(tmp_path);
        index_free(index);
        return 1;
    }
    if (rename(tmp_path, INDEX_FILE) != 0)
    {
        fprintf(stderr, "Error renaming %s to %s\n", tmp_path, INDEX_FILE);
        remove(tmp_path);
        index_free(index);
        return 1;
    }

    fprintf(stderr, "wrote %ld terms to %s\n", terms, INDEX_FILE);
    index_free(index);
    return 0;
}
