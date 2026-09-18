# ADS 课程作业

本目录保存 ADS（算法与数据结构）课程的 C 语言作业实现。

## 目录

- `hw1.c`：AVL 树插入作业。读入 `n` 个整数依次插入，插入过程中通过 LL / LR / RR / RL 旋转保持平衡，最后输出根结点的值。

## 题目描述

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

## 构建与运行

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

## 已知问题（`hw1.c` 当前版本尚未修改）

- 输入含重复值时程序崩溃，如 `1 1 1`、`2 3 3`，最少 3 个元素即可触发。
- `n = 0` 时程序崩溃。
- 输出根结点值后不带换行符；`malloc` 返回值未检查，结点内存未释放。
- 崩溃直接原因是旋转时解引用 `NULL` 子结点；文件末尾注释提出的修复方向（用子树高度判断旋转类型）经实测有效，但注释中「无限递归」的原因描述与实测不符。

## 说明

输入格式与输出要求以题目描述为准；重复值的处理方式（去重或允许重复）需按题目约定确认。
