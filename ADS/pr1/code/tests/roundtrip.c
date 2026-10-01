/* 往返校验工具：读 index.bin -> index_load() 重建内存索引 -> index_save() 再写一份。
 * 两次输出的文件应该逐字节相同（cmp 验证），证明编解码是自洽的。
 * 编译：gcc -std=c99 -Wall -Wextra -o roundtrip roundtrip.c ../stem.c -lm
 *       （索引模块在 ../index.h 里，本文件 #include 它即可）
 * 用法：./roundtrip <输入索引> <输出索引>
 */
#include <stdio.h>
#include <stdlib.h>
#include "../index.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <in.bin> <out.bin>\n", argv[0]);
        return 2;
    }
    FILE *in = fopen(argv[1], "rb");
    if (!in)
    {
        fprintf(stderr, "Error opening %s\n", argv[1]);
        return 1;
    }
    inverted_index *index = index_load(in);
    fclose(in);
    if (!index)
    {
        fprintf(stderr, "Error: %s is not a valid index\n", argv[1]);
        return 1;
    }
    FILE *out = fopen(argv[2], "wb");
    if (!out)
    {
        fprintf(stderr, "Error creating %s\n", argv[2]);
        index_free(index);
        return 1;
    }
    long terms = index_save(index, out);
    int close_failed = (fclose(out) != 0);
    index_free(index);
    if (terms < 0 || close_failed)
    {
        fprintf(stderr, "Error writing %s\n", argv[2]);
        return 1;
    }
    printf("roundtrip: %ld terms\n", terms);
    return 0;
}
