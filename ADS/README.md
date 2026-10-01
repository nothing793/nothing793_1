# ADS 课程作业

本目录保存 ADS（算法与数据结构）课程的 C 语言作业与 project 实现。

## 目录

- `hw1.c`：AVL 树插入作业。读入 `n` 个整数依次插入，插入过程中通过 LL / LR / RR / RL 旋转保持平衡，最后输出根结点的值。
- `hw2.c`：3 阶 B+ 树作业。实现初始化、插入（含分裂）、查找，并按层输出整棵树。
- `hw3.c`：Document Distance 作业。用词频向量的夹角度量两篇文档的距离；已把 [wooorm/stmr.c](https://github.com/wooorm/stmr.c) 的 Porter 词干算法内联进来，是**自包含单文件**（PTA 只提交这一个 `.c`）。仓库里另存的 `stmr.c` / `stmr.h` 仅是算法来源参考，不参与编译（MIT 许可见 `LICENSE-stmr.txt`）。
- `pr1/`：project 1「Roll Your Own Mini Search Engine」——对语料建带词干化的倒排索引，支持单词 / 短语查询与阈值实验。**交付两个源文件**：`code/index_gen.c`（切词 + 统计 + 建索引）与 `code/query.c`（查询），共用的索引模块放在头文件 `code/index.h` 里（`static inline` 实现），词干算法 `stem.c` / `stem.h` 是外部引用。说明见 [`pr1/readme.md`](pr1/readme.md)，完整报告见 [`pr1/documentation.md`](pr1/documentation.md)。

## hw1.c：AVL 树

### 题目描述

> An AVL tree is a self-balancing binary search tree. In an AVL tree, the heights of the two child subtrees of any node differ by at most one; if at any time they differ by more than one, rebalancing is done to restore this property. Figures 1-4 illustrate the rotation rules.
>
> Now given a sequence of insertions, you are supposed to tell the root of the resulting AVL tree.

题面中的 Figure 1–4 是 LL / LR / RR / RL 四种旋转的示意图，本仓库未保存这些图片。

**Input Specification**：每个输入文件包含一个测试用例。第一行是正整数 `N`（`N ≤ 20`），表示待插入的关键字个数；下一行给出 `N` 个互不相同的整数关键字，以空格分隔。

**Output Specification**：输出最终 AVL 树的根结点值，占一行。

**Sample Input 1**

```
5
88 70 61 96 120
```

**Sample Output 1**

```
70
```

**Sample Input 2**

```
7
88 70 61 96 120 90 65
```

**Sample Output 2**

```
88
```

### 构建与运行

```bash
gcc -Wall -Wextra -o hw1 hw1.c
./hw1
```

程序从标准输入读取数据：第一行是整数个数 `n`，第二行是 `n` 个整数。

```bash
printf '5\n1 2 3 4 5\n'   | ./hw1   # 输出 2
printf '5\n5 4 3 2 1\n'   | ./hw1   # 输出 4
printf '4\n10 20 30 25\n' | ./hw1   # 输出 20
```

当前版本用 `gcc -Wall -Wextra` 编译无警告，题面的两组样例均可通过（`88 70 61 96 120` → `70`，`88 70 61 96 120 90 65` → `88`）。

### 已知问题（`hw1.c` 当前版本尚未修改）

- 输入含重复值时程序崩溃，如 `1 1 1`、`2 3 3`，最少 3 个元素即可触发。
- `n = 0` 时程序崩溃。
- 输出根结点值后不带换行符；`malloc` 返回值未检查，结点内存未释放。
- 崩溃直接原因是旋转时解引用 `NULL` 子结点；文件末尾注释提出的修复方向（用子树高度判断旋转类型）经实测有效，但注释中「无限递归」的原因描述与实测不符。

### 说明

输入格式与输出要求以题目描述为准；重复值的处理方式（去重或允许重复）需按题目约定确认。

## hw2.c：3 阶 B+ 树

### 题目描述

> In this project, you are supposed to implement a B+ tree of order 3, with the following operations: initialize, insert (with splitting) and search. The B+ tree should be able to print out itself.

**Input Specification**：每个输入文件包含一个测试用例。第一行是正整数 `N`（`N ≤ 10^4`），表示待插入的关键字个数；下一行给出 `N` 个正整数关键字，以空格分隔。

**Output Specification**：把关键字按输入顺序依次插入一棵初始为空的 3 阶 B+ 树。若关键字已存在，按插入顺序输出一行 `Key X is duplicated`（不入树）。全部插入完成后，按自顶向下的层次顺序输出整棵 B+ 树，每层占一行；同一层的结点直接相连，结点形如 `[k0,k1,...]`，关键之间用逗号分隔，无空格。

**Sample Input 1**

```
6
7 8 9 10 7 4
```

**Sample Output 1**

```
Key 7 is duplicated
[9]
[4,7,8][9,10]
```

**Sample Input 2**

```
10
3 1 4 5 9 2 6 8 7 0
```

**Sample Output 2**

```
[6]
[2,4][8]
[0,1][2,3][4,5][6,7][8,9]
```

**Sample Input 3**

```
3
1 2 3
```

**Sample Output 3**

```
[1,2,3]
```

### 阶数与分裂约定

样例 1 的 `[4,7,8]` 和样例 3 的 `[1,2,3]` 说明本题「3 阶」的含义是：

- 叶结点最多 `order = 3` 个关键字，关键字本身就是数据，叶结点之间用 `next` 串成有序链表；
- 内部结点最多 `order - 1 = 2` 个关键字、`order = 3` 个孩子，`keys[i]` 是它第 `i+1` 棵子树中的最小关键字；
- 下探时若 `key >= keys[i]` 则进入第 `i+1` 个孩子（关键字等于分隔值时走右子树），否则 9 会被错误地查进 `[4,7,8]` 里；
- 上溢分裂：叶结点把 `order + 1 = 4` 个关键字从中间平分（左右各 2 个），右结点的第一个关键字提升给父结点；内部结点把中间关键字提升给父结点，左右各留一半；
- 根结点分裂时新建一个根，树长高一层；所有叶结点始终位于同一层，因此按层输出即可。

### 实现结构（`hw2.c`）

| 函数 | 作用 |
| --- | --- |
| `initialize` | 把根指针置空，得到一棵空树 |
| `search` | 从根下探到叶结点，判断关键字是否存在 |
| `leafinsert` / `internalinsert` | 分别在叶结点、内部结点完成「插入 + 必要时分裂」，通过 `*upkey` / `*right` 把提升关键字和右兄弟回传给上层 |
| `insertrec` | 递归插入，返回 `-1`（重复）/ `0`（无分裂）/ `1`（发生分裂），分裂信息逐层向上回传 |
| `insert` | 对外接口：处理空树、重复关键字提示、根分裂长高 |
| `print_tree` | 层次遍历输出，逐层收集结点到下一层队列，每层一行 |
| `free_tree` | 结束前释放所有结点 |

### 构建与运行

```bash
gcc -Wall -Wextra -o hw2 hw2.c

printf '6\n7 8 9 10 7 4\n'            | ./hw2   # 样例 1
printf '10\n3 1 4 5 9 2 6 8 7 0\n'    | ./hw2   # 样例 2
printf '3\n1 2 3\n'                   | ./hw2   # 样例 3
```

当前版本用 `gcc -Wall -Wextra` 编译无警告，题面三组样例输出与预期完全一致；另外用随机用例校验过叶结点关键字序列等于去重后的升序全集、每个分隔关键字等于其右子树的最小关键字、结点容量不超限等不变量，并在 `-fsanitize=address,undefined` 下无越界、无未定义行为、无内存泄漏。`N = 10^4` 实测约 3 ms（树高约 13 层）。

### 已知限制

- 只实现插入与查找，没有实现删除。
- 结点数组容量按 `order` 写死为常量，若将来改成其它阶数，需要同时调整分裂点（叶结点按 `order+1` 平分、内部结点取中间关键字提升）。
- 题目保证 `N ≤ 10^4`，未针对更大的输入做特别优化。

### 说明

本题的重复关键字按题面处理：已在树中的关键字不再插入，并输出 `Key X is duplicated`（与 hw1 的 AVL 树题目不同，那道题保证关键字互不相同）。

## hw3.c：Document Distance

### 题目描述

> Plagiarism is a form of academic dishonesty. To fight with such misconducts, plagiarism checkers are developed to compare any submitted article with all the articles stored in a database. The key is that given two documents, how to judge their similarity, or in other words, document distance?
>
> … Your stemming algorithm must be able to handle "es", "ed", "ing", "ies" and must be case-insensitive. Stop words are not supposed to be ignored and must be treated as normal words.

术语与度量：

- **Word**：连续的数字/字母序列，题面保证单词不超过 20 个字符。
- **Word frequency**：`F_D(w)` 表示词 `w` 在文档 `D` 中出现的次数。
- **Document distance metric**：`(F(D1),F(D2)) = Σ F1(w)·F2(w)`（词频向量内积）。
- **Angle metric**：`θ(D1,D2) = arccos( (F(D1),F(D2)) / (|F(D1)|·|F(D2)|) ) ∈ [0,π/2]`，其中 `|F(D)| = sqrt((F(D),F(D)))`（2-范数）。

**Input Specification**：第一行是正整数 `N（N ≤ 100）`，表示待处理的文本文件个数；随后 `N` 个文件块，每块第一行是文章题名（不超过 6 个字符、不含空格），接着是正文若干行，最后一行是单独的字符 `#`；文件块结束后有一行正整数 `M（M ≤ 100,000）`，随后 `M` 行询问，每行是两个题名，用空格分隔。最大的测试用例约 1 MB。

**Output Specification**：每个询问输出一行 `Case #: *`，其中 `#` 是询问编号（从 1 开始），`*` 是文档距离，保留 3 位小数。

**Sample Input**

```
3
A00
A B C
#
A01
B C D
#
A02
A C
D A
#
2
A00 A01
A00 A02
```

**Sample Output**

```
Case 1: 0.841
Case 2: 0.785
```

### 实现要点（`hw3.c`）

| 函数 / 结构 | 作用 |
| --- | --- |
| `WordEntry` | 一个词：`word` / `freq` / `bucketnext`（桶内链）/ `listnext`（文档内全词链） |
| `Document` | 一篇文档：`title`、`size`（不同单词数）、2048 个哈希桶、全词链表头 |
| `hashstring` | djb2 字符串哈希 |
| `stemword` | 统一转小写后调用内联的 `stem()` 做 Porter 词干提取 |
| `lookup` / `addword` | 哈希表查词 / 词频自增或插入，均摊 O(1) |
| `scanline` | 把一行文本按「连续 alnum 段」切成单词 |
| `isterminator` | 判断是否是独占一行的 `#`（容忍前后空格与 `\r`） |
| `dotproduct` | 遍历词少的那篇文档，在另一篇里查表求内积 |
| `finddocument` | 题名 → 文档下标 的哈希查找，O(1) |

关键设计：

1. **词切分按字符扫描而不是按空白切分**：题面把词定义为「连续的数字/字母序列」，所以标点必须当分隔符（`"don't"` → `don` / `t`，`"well-known"` → `well` / `known`），用 `scanf("%s")` 会把 `test-case` 当成一个词。
2. **词干 + 大小写不敏感**：先 `tolower` 再 `stem()`；`stmr.c` 只处理小写字母序列，且 `stem(p,0,len-1)` 返回词干末字符下标、不写结束符，因此调用后要自己补 `'\0'`。停用词照常统计。
3. **所有文件预先两两算好**：`N ≤ 100`，文件对最多 `C(100,2) = 4950` 个，先把点积矩阵和 2-范数算完，之后每次询问只做常数次浮点运算。若逐个询问现算，最坏 `10^5` 次询问 × 上千个词会超时。
4. **浮点安全**：余弦值夹到 `[-1,1]` 再 `acos`，避免浮点误差导致 `nan`；两篇都为空约定 `0.000`，一篇为空的夹角为 `π/2`。

### 构建与运行

```bash
gcc -std=c99 -Wall -Wextra -O2 -o hw3 hw3.c -lm

printf '3\nA00\nA B C\n#\nA01\nB C D\n#\nA02\nA C\nD A\n#\n2\nA00 A01\nA00 A02\n' | ./hw3
```

只编译 `hw3.c` 即可（不用带 `stmr.c`），在 `-std=c99`、`-std=gnu99`、`-std=c11`、`-std=gnu11`、`-std=gnu17` 下用 `gcc -Wall -Wextra` 编译均无警告，`-Wshadow` 亦无警告。

`hw3.c` 自带完整注释：自己的部分用中文注释，内联的 Porter 词干部分保留上游英文注释与算法出处。

### 验证结果

- 题面样例：`0.841`、`0.785`，与预期一致（手算：`2/3 → 0.8411`，`3/(√3·√6) → 0.7854`）。
- 与一份独立的 Python 参考实现（切词、词频、夹角独立实现，仅共用同一个 `stem` 程序）对拍：60 组随机小用例 + 1 组中规模用例（20 篇 × 3000 词、500 询问）输出完全一致。
- `-fsanitize=address,undefined` 下 9 组用例（样例、空文档、同一篇自比、CRLF 输入、末尾无换行、超长单词、1.9 MB 用例等）无越界、无未定义行为、无内存泄漏。
- 内联前后等价性：把 `stmr.c` 内联进 `hw3.c` 后，10 个用例（含 22 MB / 2.2 M 词、1.9 MB 加 10 万询问的用例）的输出与原「`hw3.c` + `stmr.c`」两文件版本逐字节一致。
- 规模：22 MB 输入（100 篇 × 2 万词、10 万询问）耗时 1.8 s；按题面上限构造的 1.9 MB 用例（100 篇 × 1000 词、10 万询问）耗时 0.03 s。

### 词干算法（`stmr.c`）

本题按作业要求采用 [wooorm/stmr.c](https://github.com/wooorm/stmr.c) 的 Porter 词干算法，`hw3.c` 只保留这一种实现（已内联，提交单文件即可）：

- 题面 Note 要求能处理 `es` / `ed` / `ing` / `ies`，Porter 全部覆盖：`runs` → `run`、`cats` → `cat`、`studies` → `studi`、`walked` → `walk`、`nationalization` → `nation`。
- 它还会继续处理 `-ation` / `-ly` / `-er` / `-al` / `-ment` / `-ous` 等更多后缀（例如 `quickly` → `quick`、`computer` → `comput`），这是该算法的固有行为。
- 大小写由 `hw3.c` 的 `stemword()` 先 `tolower` 保证（`stem()` 只处理小写字母序列），调用后再补 `'\0'`。

同一份数据的实测差异（用 GPL-3 正文的前后两半作两篇文档，仅说明词干策略的影响，不是本题结果）：

| 处理方式 | 夹角 | 输出的 3 位小数 |
| --- | --- | --- |
| 不做词干化 | 0.388596 | 0.389 |
| 只剥离 `es` / `ed` / `ing` / `ies` | 0.388626 | 0.389 |
| **Porter（本题采用）** | 0.399823 | **0.400** |

可以看到词干策略会明显改变第 3 位小数，所以不能用「简单后缀剥离」之类的实现替代 `stmr.c`。另外样例数据里没有需要词干化的词，仅靠样例无法区分不同的词干方案。

### 附：内联的 `stmr.c` / `stmr.h`

- 来源：<https://github.com/wooorm/stmr.c>，Martin Porter 1980 年词干算法的 ANSI C 实现（MIT 许可，原 `license` 文件保留为 `LICENSE-stmr.txt`）。
- 接口：`int stem(char *p, int index, int position)`，在 `p[index..position]` 上原地做词干提取，返回词干末字符的下标（不写结束符），长度 ≤ 2 的串原样返回。
- 调用约定：只对小写字母序列生效（所以先 `tolower`），调用后需自行补 `'\0'`；内部使用静态变量，不可重入、非线程安全（本题单线程使用，无影响）。
- 内联时的改动（仅此几处，算法逻辑未动）：
  - 去掉 `#include "stmr.h"`，把 `TRUE` / `FALSE` 宏与 `stem()` 的原型写进 `hw3.c`；
  - `stem()` 由外部链接改为 `static`（单文件不需要对外导出，也不会和 `stmr.c` 重复定义）；
  - 原有文件级静态变量 `b` / `k` / `k0` / `j` 改名为 `stem_buf` / `stem_k` / `stem_k0` / `stem_j`，避免与 `hw3.c` 里的局部变量同名遮蔽；
  - 保留上游注释（算法出处、`--DEPARTURE--` 说明等），并在段首补了一段说明内联改动与变量改名的中文注释。
- 仓库里的 `stmr.c` / `stmr.h` 保持上游原样，仅作为来源对照，编译时不需要。

### 已知限制

- 单词超过 20 个字符时只取前 20 个字符（题面保证不会出现）。
- 询问中若出现不存在的题名，按距离 `0.000` 输出（题面保证询问的文件都存在，这里只作防御）。
- 只按 ASCII 的 `isalnum` 切词，非 ASCII 字节会被当作分隔符。
- 文档数按 `N ≤ 100` 设计（哈希桶数 2048、点积矩阵 `N × N`），`N` 更大时需调大 `BUCKETS`。

## pr1：迷你搜索引擎（Roll Your Own Mini Search Engine）

project 1 的实现独立放在 [`pr1/`](pr1/) 目录下，不参与 `hw*.c` 的编译：

- [`pr1/readme.md`](pr1/readme.md)：题面、目录用途、构建与运行、输入输出约定、注意事项。
- [`pr1/documentation.md`](pr1/documentation.md)：完整报告（题目分析、方法、实验、验证、Bonus 讨论）。
- [`pr1/code/README.md`](pr1/code/README.md)：代码目录说明（设计取舍、`index.bin` 格式规范、踩坑记录）。
- [`pr1/code/tests/README.md`](pr1/code/tests/README.md)：测试与实验脚本说明。

要点：

- 交付的源文件只有两个 —— `code/index_gen.c`（切词 + Part 1 词频/停用词统计 + Part 2 建索引）与
  `code/query.c`（Part 3 单词 / AND / 短语查询 + Part 4 查询阈值 τ）；词干算法 `code/stem.c` 是外部引用。
- 两者共用的索引模块（内存索引、`index.bin` 读写、以及**格式规范注释**）整体放在 `code/index.h` 里，
  函数写成 `static inline`，两个程序各 `#include` 一次：格式在磁盘上只有一份定义，两个程序各自编译一份副本。
- 构建（`cd pr1/code` 后执行）：

  ```bash
  gcc -std=c99 -Wall -Wextra -o index_gen index_gen.c stem.c -lm
  gcc -std=c99 -Wall -Wextra -o query     query.c     stem.c -lm
  ./index_gen 语料文件...        # 每个文件 = 一篇文档，产出 index.bin + stoplist.txt
  ./query 词1 词2                # 查询；不带参数则读 test.txt，结果写 output.txt
  ```

- 测试：`cd pr1/code/tests && sh run_tests.sh`（在临时目录里编译并跑完整流水线，当前 21 项全部通过，
  四个程序在 `-std=c99 -Wall -Wextra -Wpedantic` 下零告警）。
