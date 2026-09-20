# ADS 课程作业

本目录保存 ADS（算法与数据结构）课程的 C 语言作业实现。

## 目录

- `hw1.c`：AVL 树插入作业。读入 `n` 个整数依次插入，插入过程中通过 LL / LR / RR / RL 旋转保持平衡，最后输出根结点的值。
- `hw2.c`：3 阶 B+ 树作业。实现初始化、插入（含分裂）、查找，并按层输出整棵树。

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
