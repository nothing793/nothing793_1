/* 词干命令行工具：从 stdin 读空白分隔的词，逐行输出词干。
 * 只给 brute_check.py 用 —— Python 侧没有 Porter 实现，借它把 file.txt 里的词
 * 词干化，才能和索引内容独立对拍。
 * 编译：gcc -std=c99 -Wall -Wextra -o stem_list stem_list.c ../stem.c -lm
 */
#include <stdio.h>
#include <string.h>
#include "../stem.h"

int main(void)
{
    char word[256];
    while (scanf("%255s", word) == 1)
    {
        stemword(word);
        printf("%s\n", word);
    }
    return 0;
}
