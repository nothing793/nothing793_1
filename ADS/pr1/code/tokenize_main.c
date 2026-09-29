/* pr1 切词命令行：原始文本 -> index_gen 消费的三元组流（file.txt）。
 *
 * 用法：
 *   ./tokenize 文件1 文件2 ... > file.txt   # 每个文件 = 一篇文档，doc_id 按命令行顺序 0,1,2...
 *   ./tokenize -                            # 从 stdin 读一篇文档（doc_id = 0）
 *
 *   ./tokenize 只负责切词 + 转小写，不做词干化（原因见 tokenize.h）；
 *   词干化由 index_gen / query 在做索引和查询时各做一次。
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o tokenize tokenize_main.c tokenize.c
 */
#include <stdio.h>
#include <string.h>

#include "tokenize.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <file>...   (each file becomes one document, doc_id = 0,1,2...)\n"
                        "       %s -          (read one document from stdin)\n", argv[0], argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; i++)
    {
        FILE *in;
        if (strcmp(argv[i], "-") == 0)
        {
            in = stdin;
        }
        else
        {
            in = fopen(argv[i], "r");
            if (!in)
            {
                fprintf(stderr, "Error opening %s\n", argv[i]);
                return 1;
            }
        }

        long words = tokenize_file(in, i - 1, stdout);
        int read_failed = ferror(in);
        if (in != stdin)
        {
            fclose(in);
        }
        if (words < 0 || read_failed)
        {
            fprintf(stderr, "Error tokenizing %s\n", argv[i]);
            return 1;
        }
        fprintf(stderr, "doc_id %d <- %s: %ld words\n", i - 1, argv[i], words);
    }

    if (fflush(stdout) != 0 || ferror(stdout))
    {
        fprintf(stderr, "Error writing to stdout\n");
        return 1;
    }
    return 0;
}
