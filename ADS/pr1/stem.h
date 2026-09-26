/* pr1（8-1 Roll Your Own Mini Search Engine）词干模块的头文件。
 *
 * 与 stem.c 配对：编译时把 stem.c 一起交给编译器（或链上它的 .o）即可，
 * 不需要额外的链接参数。本文件只声明接口，可以被任意多个 .c 重复 include。
 *
 */

#ifndef PR1_STEM_H
#define PR1_STEM_H

/* 在 p[index..position] 上原地做 Porter 词干提取，返回词干末字符的下标，
 * 不写结束符；长度 ≤ 2 的字符串原样返回。只对小写字母序列有效，所以调用前
 * 需要调用方自己统一大小写。 */
int stem(char *p, int index, int position);

/* 转小写 + 词干提取，原地处理以 '\0' 结尾的字符串。
 * 会改写传入的缓冲区：不要传字符串字面量或 const char *。 */
void stemword(char *word);

#endif /* PR1_STEM_H */
