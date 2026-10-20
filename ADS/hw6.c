/*
 * 题目：Werewolf（狼人杀）
 *
 * N 个玩家中混有 M 只狼人。第 i 个玩家指认一名玩家：正号表示“被指认者是
 * 人类”，负号表示“被指认者是狼人”。已知恰好有 L 个说谎者，并且至少有
 * 一只、但并非全部狼人在说谎，求按降序排列最大的那组狼人；无解输出
 * No Solution。
 *
 * 输入（单个测试用例）：
 *   第一行：N M L，其中 5 ≤ N ≤ 100，2 ≤ M < N，1 ≤ L < N；
 *   随后 N 行：第 i 行为第 i 个玩家的发言（每行一个带符号的编号）。
 *
 * 输出：
 *   按降序输出 M 个狼人编号，数字之间用一个空格分隔，行首行尾无多余空格；
 *   若无解，输出 No Solution。
 *
 * 样例：
 *   输入            输出
 *   5 2 2           4 1
 *   -2 +3 -4 +5 +4
 *
 *   6 2 3           6 4
 *   -2 +3 -4 +5 +4 -3
 *
 *   6 2 5           No Solution
 *   -2 +3 -4 +5 +4 +6
 */

#include <stdio.h>
#include <stdlib.h>

#define MAXN 105        /* 最大玩家数 */
#define INF  1000000    /* 权重和不可达的标记 */

int N, M, L;             /* 玩家数、狼人数、说谎人数 */
int D;                   /* 狼人权重和应等于的值：L - ΣA */
int statement[MAXN];     /* statement[i]：玩家 i 的发言 */
int weight[MAXN];        /* weight[j] = B[j] - A[j] */
int is_wolf[MAXN];       /* 搜索过程中玩家是否为狼人 */
int min_sum[MAXN][MAXN]; /* min_sum[i][k]：玩家 1..i 中恰选 k 人时的最小权重和 */
int max_sum[MAXN][MAXN]; /* max_sum[i][k]：玩家 1..i 中恰选 k 人时的最大权重和 */

/* 校验当前狼人集合是否满足全部条件 */
int check_solution(void)
{
    int liars = 0, wolf_lie = 0, wolf_truth = 0;
    int i, target, truth;

    for (i = 1; i <= N; i++)
    {
        target = abs(statement[i]);
        truth = (is_wolf[target] == (statement[i] < 0)); /* 发言与真实身份一致即为真话 */
        if (!truth)
        {
            liars++;
            if (is_wolf[i]) wolf_lie = 1;   /* 有狼人在说谎 */
        }
        else if (is_wolf[i])
        {
            wolf_truth = 1;                 /* 有狼人在说真话 */
        }
    }
    return liars == L && wolf_lie && wolf_truth;
}

/* 从大到小决定每个玩家是否为狼人：先选后不选，保证得到的解字典序最大 */
int dfs(int i, int cnt, int sum)
{
    int need, j;

    if (cnt == M)   /* 狼人已选满，剩下的人都当人类 */
    {
        if (sum != D) return 0;
        for (j = 1; j <= i; j++) is_wolf[j] = 0;
        return check_solution();
    }
    if (i == 0) return 0;

    need = M - cnt;
    if (need > i || min_sum[i][need] == INF) return 0;
    if (sum + min_sum[i][need] > D || sum + max_sum[i][need] < D) return 0;

    is_wolf[i] = 1;
    if (dfs(i - 1, cnt + 1, sum + weight[i])) return 1;
    is_wolf[i] = 0;
    return dfs(i - 1, cnt, sum);
}

/* 预处理 min_sum / max_sum 表，供 dfs 剪枝使用 */
void build_bound_table(void)
{
    int i, k, value;

    for (k = 0; k <= M; k++)
    {
        min_sum[0][k] = (k == 0) ? 0 : INF;
        max_sum[0][k] = (k == 0) ? 0 : -INF;
    }
    for (i = 1; i <= N; i++)
    {
        min_sum[i][0] = 0;
        max_sum[i][0] = 0;
        for (k = 1; k <= M; k++)
        {
            min_sum[i][k] = min_sum[i - 1][k];
            max_sum[i][k] = max_sum[i - 1][k];
            if (min_sum[i - 1][k - 1] == INF) continue;
            value = min_sum[i - 1][k - 1] + weight[i];
            if (value < min_sum[i][k]) min_sum[i][k] = value;
            value = max_sum[i - 1][k - 1] + weight[i];
            if (value > max_sum[i][k]) max_sum[i][k] = value;
        }
    }
}

/* 按降序输出狼人编号，行首行尾无多余空格 */
void print_answer(void)
{
    int i, first = 1;

    for (i = N; i >= 1; i--)
    {
        if (!is_wolf[i]) continue;
        if (first)
        {
            printf("%d", i);
            first = 0;
        }
        else
        {
            printf(" %d", i);
        }
    }
    printf("\n");
}

int main(void)
{
    int i, j, target, base;
    int count_wolf[MAXN] = {0};  /* 说 j 是狼人的发言条数 */
    int count_human[MAXN] = {0}; /* 说 j 是人类的发言条数 */

    if (scanf("%d %d %d", &N, &M, &L) != 3) return 0;
    for (i = 1; i <= N; i++)
    {
        if (scanf("%d", &statement[i]) != 1) return 0;
        target = abs(statement[i]);
        if (statement[i] < 0) count_wolf[target]++;
        else count_human[target]++;
    }

    base = 0;
    for (j = 1; j <= N; j++)
    {
        base += count_wolf[j];
        weight[j] = count_human[j] - count_wolf[j];
    }
    D = L - base;

    build_bound_table();
    if (!dfs(N, 0, 0))
    {
        printf("No Solution\n");
        return 0;
    }
    print_answer();
    return 0;
}
