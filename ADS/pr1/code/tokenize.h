/* pr1（8-1 Roll Your Own Mini Search Engine）切词模块的头文件。
 *
 * 职责：把莎士比亚原文切词，产出 index_gen 直接消费的三元组流 —— 每行
 *       "<word> <doc_id> <pos>"，也就是 file.txt 的内容。
 *       本模块不建索引、不做统计；index_gen / query 都与它无依赖
 *       （那两个程序只碰 file.txt 与 index.bin）。
 *
 * ⚠ 一条必须遵守的约定：本模块不做词干化，输出的是"转小写后的词形"。
 *    词干化只允许发生一次。index_gen 读 file.txt 时会对每个词调 stemword()，
 *    query 查词时也会调一次；如果这一层先把词干写进 file.txt，index_gen 就会对
 *    "已经词干化的串"再切一次。Porter 不幂等 —— 实测 604 个词里有 11 个二次切
 *    会变（because -> becaus -> becau、release -> releas -> relea、
 *    license -> licens -> licen），于是索引里存 "becau"、查询侧得 "becaus"：
 *    查不到，而且从 index.bin 本身看不出任何异常。
 *    所以词干化统一由 stem.h 的 stemword() 承担，全流程只做一次。
 *
 * 切词口径（code/README.md 第 8 节；报告里引用这一条即可）：
 *   1. 按 isalnum() 连续段切词，其余字符（空白、换行、标点）一律是分隔符：
 *      "don't" -> "don" / "t"；"well-known" -> "well" / "known"；
 *   2. 纯数字串也当词处理（题面允许）；
 *   3. 每个词只做 tolower()，保留原形（见上面的约定）；
 *   4. pos 是词在文档内的出现序号，从 0 开始、按出现顺序递增；同一个词重复出现各占一个
 *      pos（index_gen 的短语连续性验证依赖这个编号连续）；
 *   5. 超过 TOKEN_MAX-1 字节的超长 isalnum 段按 TOKEN_MAX-1 截断，不报错也不跳过：
 *      让这里与 index_gen 的 fscanf("%255s") 认定的词完全相同。否则 300 字节的段会被
 *      %255s 截成 255 字节，剩下 45 字节被当成新的一个词接着解析，后面的 doc_id / pos
 *      全部错位，fscanf 返回 1 → 该行之后的全部内容都不再入库。
 *
 * 状态：接口已定，实现未做。现有 tokenize.c 是 16 行骨架，签名与本头文件不一致
 *       （它只把句子按空格切开、什么都没写出），写实现时会被整体替换。
 */

#ifndef PR1_TOKENIZE_H
#define PR1_TOKENIZE_H

#include <stdio.h>

/* 单个词的最大字节数（含结尾 '\0'）。与 index.h 的 maxword 保持一致，
 * 让两侧对"同一个词的边界在哪"不会出现分歧。 */
#define TOKEN_MAX 256

/* 低层原语：从 *text 开始，跳过所有非 isalnum 字符，取出下一个词。
 *
 * 成功：把该词转小写后写进 word（调用方保证容量 ≥ TOKEN_MAX），返回指向
 *       "本次取词之后"的位置，调用方拿它继续取下一个词；
 * 已无词可取：返回 NULL（此时 word 的内容不作保证）。
 *
 * 不修改 text、不分配内存、不用文件级静态状态，可重入；一次取词遇到 '\0' 即停，
 * 一定不会越过 text 的结尾；按口径 5 截断。
 *
 * tokenize_text() / tokenize_file() 内部也用它；单独暴露是为了能直接对拍切词口径
 * （例如与 `tr -cs 'A-Za-z0-9' '\n'` 的结果逐词比较，不需要建临时文件）。
 */
const char *tokenize_next(const char *text, char *word);

/* 切一段原文，把三元组逐行写进 out（每行 "<word> <doc_id> <pos>\n"）。
 *
 * pos 从 pos_start 开始递增 —— 同一篇文档分多次调用时，调用方用
 * pos_start += 返回值 保持编号连续。返回成功写出的词数（可以是 0）；写 out 失败返回 -1。
 * text 不必以 '\n' 结尾，函数也不会修改它，整段一次传进来即可。
 *
 * 约定：out 是否打开成功由调用方负责；本函数只在写失败时返回 -1，并把 ferror(out)
 * 留给调用方判断（与 index_save() 的返回值约定一致）。
 */
long tokenize_text(const char *text, int doc_id, int pos_start, FILE *out);

/* 切一整个文件流（= 一篇文档），pos 从 0 开始，输出格式与 tokenize_text() 相同。
 * 返回词数；写 out 失败返回 -1。读 in 失败不在返回值里体现，由调用方检查 ferror(in)
 * （与 index_build() 的约定一致）。 */
long tokenize_file(FILE *in, int doc_id, FILE *out);

#endif /* PR1_TOKENIZE_H */
