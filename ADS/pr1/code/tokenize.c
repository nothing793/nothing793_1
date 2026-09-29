/* pr1（8-1 Roll Your Own Mini Search Engine）切词模块的实现文件。
 *
 * 职责：把原始文本切成词，产出 index_gen 直接消费的三元组流 —— 每行
 *       "<word> <doc_id> <pos>"，也就是 file.txt 的内容。
 * 切词口径、接口约定、以及"这一层不做词干化"的理由都在 tokenize.h 顶部。
 *
 * 依赖：只用标准 C（<ctype.h> / <stdio.h> / <string.h>）与 tokenize.h。
 *       **不要** include stem.h —— 这一层不做词干化。
 *
 * 编译：gcc -std=c99 -Wall -Wextra -o tokenize tokenize_main.c tokenize.c
 *       （命令行入口在 tokenize_main.c；index_gen / query 不依赖本模块）
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "tokenize.h"

const char *tokenize_next(const char *text, char *word)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t n = 0;

    /* 口径 1：跳过所有非 isalnum 字符（空白、换行、标点都是分隔符） */
    while (*p != '\0' && !isalnum(*p))
    {
        p++;
    }
    if (*p == '\0')
    {
        return NULL;
    }

    /* 口径 2/3/5：收 isalnum 连续段并转小写；超过 TOKEN_MAX-1 字节的部分只推进扫描位置 */
    while (*p != '\0' && isalnum(*p))
    {
        if (n + 1 < TOKEN_MAX)
        {
            word[n++] = (char)tolower(*p);
        }
        p++;
    }
    word[n] = '\0';
    return (const char *)p;
}

long tokenize_text(const char *text, int doc_id, int pos_start, FILE *out)
{
    char word[TOKEN_MAX];
    const char *p = text;
    int pos = pos_start;
    long written = 0;

    while ((p = tokenize_next(p, word)) != NULL)
    {
        if (fprintf(out, "%s %d %d\n", word, doc_id, pos) < 0)
        {
            return -1;
        }
        pos++;
        written++;
    }
    return written;
}

long tokenize_file(FILE *in, int doc_id, FILE *out)
{
    char word[TOKEN_MAX];
    size_t n = 0;
    int pos = 0;
    int c;
    long written = 0;

    /* 逐字符读：跨行的词自然会被接上（行尾换行只是分隔符），
     * 也不需要假设一行的长度上限。截断口径与 tokenize_next() 一致。 */
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
            if (fprintf(out, "%s %d %d\n", word, doc_id, pos) < 0)
            {
                return -1;
            }
            pos++;
            written++;
            n = 0;
        }
    }

    if (n > 0)   /* 文件末尾没有分隔符：把最后一个词补出去 */
    {
        word[n] = '\0';
        if (fprintf(out, "%s %d %d\n", word, doc_id, pos) < 0)
        {
            return -1;
        }
        written++;
    }
    return written;
}
