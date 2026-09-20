#include <stdio.h>
#include <stdlib.h>

/*  3 阶 B+ 树
 *  ---------------------------------------------------------------
 *  约定（与题面样例一致）：
 *    - 叶结点最多 order = 3 个关键字，关键字即为数据，
 *      叶结点之间用 next 指针串成有序链表；
 *    - 内部结点最多 order - 1 = 2 个关键字、order = 3 个孩子，
 *      keys[i] 是第 i + 1 棵子树中的最小关键字，children[i] 指向该子树；
 *    - 下探时若 key >= keys[i] 就走第 i + 1 个孩子，
 *      即关键字等于分隔值时要进入右子树，否则会查错子树；
 *    - 上溢分裂：叶结点把 order + 1 = 4 个关键字对半平分，右结点第一个关键字提升；
 *      内部结点把中间关键字提升，左右各留一半；
 *    - 根分裂时新建根，树长高一层，所有叶结点始终位于同一层。
 *  ---------------------------------------------------------------
 */

#define order 3                  /* B+ 树的阶数：叶结点最大关键字的个数 */
#define maxleaf order             /* 叶结点关键字上限 */
#define maxint (order - 1)        /* 内部结点关键字上限（孩子数为 order） */

/* B+ 树结点：叶结点与内部结点共用同一个结构，用 isleaf 区分，
 * 二者分别使用联合体中的 leaf / internal 成员。 */
typedef struct BPtreenode
{
    int isleaf;                  /* 1 表示叶结点，0 表示内部结点 */
    int numkeys;                 /* 当前关键字个数（叶结点为数据个数） */
    int keys[order + 1];         /* 关键字数组；多留 1 个位置给插入时的临时溢出 */
    union
    {
        struct leaf
        {
            int values[order + 1];       /* 叶结点中的数据（与 keys 同步，便于输出） */
            struct BPtreenode *next;     /* 指向下一个叶结点，组成有序链表 */
        } leaf;
        struct internal
        {
            struct BPtreenode *children[order + 2];  /* 子结点指针，多留 1 个位置 */
        } internal;
    } u;
} BPtreenode;

/* B+ 树本身只保存一个根指针，根为空即空树 */
typedef struct BPTree
{
    BPtreenode *root;
} BPTree;

/* 初始化：把根指针置空，得到一棵空树 */
void initialize(BPTree *tree)
{
    tree->root = NULL;
}

/* 新建一个空的叶结点；分配失败返回 NULL */
static BPtreenode *newleaf(void)
{
    BPtreenode *node = (BPtreenode *)malloc(sizeof(BPtreenode));
    if (node == NULL)
        return NULL;
    node->isleaf = 1;
    node->numkeys = 0;
    node->u.leaf.next = NULL;
    return node;
}

/* 新建一个空的内部结点，孩子指针全部置空；分配失败返回 NULL */
static BPtreenode *newinternal(void)
{
    BPtreenode *node = (BPtreenode *)malloc(sizeof(BPtreenode));
    if (node == NULL)
        return NULL;
    node->isleaf = 0;
    node->numkeys = 0;
    for (int i = 0; i <= order + 1; i++)
        node->u.internal.children[i] = NULL;
    return node;
}

/* 查找：关键字存在返回 1，否则返回 0。
 * 内部结点中满足 key >= keys[i] 就继续往右走，与插入时的下探规则保持一致，
 * 保证同一个关键字在查找与插入路径上进入同一棵子树。 */
int search(BPTree *tree, int key)
{
    BPtreenode *node = tree->root;

    /* 自根向下，一直走到叶结点 */
    while (node != NULL && !node->isleaf)
    {
        int i = 0;
        while (i < node->numkeys && key >= node->keys[i])
            i++;
        node = node->u.internal.children[i];
    }
    if (node == NULL)
        return 0;
    /* 叶结点中顺序查找，关键字与数据一致，比较任意一个即可 */
    for (int i = 0; i < node->numkeys; i++)
        if (node->u.leaf.values[i] == key)
            return 1;
    return 0;
}

/* 叶结点插入：把 key 插入有序位置，必要时分裂。
 * 无需分裂时返回 0；发生分裂时返回 1，并通过 *upkey 回传要提升到父结点的关键字、
 * 通过 *right 回传新产生的右兄弟结点。 */
static int leafinsert(BPtreenode *node, int key, int *upkey, BPtreenode **right)
{
    /* 从后往前比较，把比 key 大的关键字（及对应数据）整体后移一位 */
    int i = node->numkeys - 1;

    while (i >= 0 && node->keys[i] > key)
    {
        node->keys[i + 1] = node->keys[i];
        node->u.leaf.values[i + 1] = node->u.leaf.values[i];
        i--;
    }
    node->keys[i + 1] = key;
    node->u.leaf.values[i + 1] = key;   /* 叶结点中关键字本身就是数据 */
    node->numkeys++;

    /* 未超过阶数上限，无需分裂 */
    if (node->numkeys <= maxleaf)
        return 0;

    /* 分裂点：order + 1 个关键字对半平分，左右各 2 个 */
    int mid = node->numkeys / 2;
    BPtreenode *newnode = newleaf();

    /* 把后一半（含 mid 位置起）的关键字搬到新结点 */
    newnode->numkeys = node->numkeys - mid;
    for (int j = 0; j < newnode->numkeys; j++)
    {
        newnode->keys[j] = node->keys[mid + j];
        newnode->u.leaf.values[j] = node->u.leaf.values[mid + j];
    }
    node->numkeys = mid;                 /* 原结点只保留前一半 */

    /* 维持叶结点链表的顺序：原结点 -> 新结点 -> 原来的后继 */
    newnode->u.leaf.next = node->u.leaf.next;
    node->u.leaf.next = newnode;

    *upkey = newnode->keys[0];           /* 右结点的最小关键字提升给父结点 */
    *right = newnode;
    return 1;
}

/* 内部结点插入：在第 pos 个孩子插入新子树后触发。
 * key 为下层提升上来的关键字，rightchild 为新分裂出的右子树。
 * 返回值与回传参数的含义同 leafinsert。 */
static int internalinsert(BPtreenode *node, int pos, int key, BPtreenode *rightchild,
                          int *upkey, BPtreenode **right)
{
    int n = node->numkeys;

    /* 为腾出 pos 位置，把 pos 之后的关键字和指向子树的指针整体后移；
     * 关键字 keys[i] 比孩子 pointers[i + 1] 先移动，避免覆盖 */
    for (int i = n; i > pos; i--)
    {
        node->keys[i] = node->keys[i - 1];
        node->u.internal.children[i + 1] = node->u.internal.children[i];
    }
    node->keys[pos] = key;                            /* 提升上来的关键字插在 pos */
    node->u.internal.children[pos + 1] = rightchild;  /* 新子树叶排在原第 pos 个孩子右侧 */
    node->numkeys = n + 1;

    /* 未超过内部结点关键字上限，无需分裂 */
    if (node->numkeys <= maxint)
        return 0;

    /* 分裂：中间关键字 keys[mid] 提升给父结点，左右各留一半关键字 */
    int mid = node->numkeys / 2;
    BPtreenode *newnode = newinternal();

    *upkey = node->keys[mid];

    newnode->numkeys = node->numkeys - mid - 1;   /* 右结点关键字个数（不含提升的那个） */
    for (int j = 0; j < newnode->numkeys; j++)
    {
        newnode->keys[j] = node->keys[mid + 1 + j];
        newnode->u.internal.children[j] = node->u.internal.children[mid + 1 + j];
    }
    /* 右结点最后多一个孩子：原结点原来最右侧的孩子跟着提升关键字一起过去 */
    newnode->u.internal.children[newnode->numkeys] = node->u.internal.children[node->numkeys];
    node->numkeys = mid;                          /* 左结点保留提升关键字左侧的部分 */

    *right = newnode;
    return 1;
}

/* 递归插入：沿路径下探到叶结点后插入，若下层发生分裂就把提升关键字插到当前结点。
 * 返回值：-1 表示关键字已存在（重复，不插入）；0 表示插入成功且未分裂；
 *         1 表示插入成功且当前结点分裂（分裂信息通过 *upkey / *right 回传）。 */
static int insertrec(BPtreenode *node, int key, int *upkey, BPtreenode **right)
{
    /* 到达叶结点：先判重，再由 leafinsert 完成插入与可能的分裂 */
    if (node->isleaf)
    {
        for (int i = 0; i < node->numkeys; i++)
            if (node->u.leaf.values[i] == key)
                return -1;               /* 关键字重复，交给 insert 打印提示 */
        return leafinsert(node, key, upkey, right);
    }

    /* 内部结点：按插入时的下探规则选择孩子，key >= keys[i] 选第 i + 1 个孩子 */
    int i = 0;
    while (i < node->numkeys && key >= node->keys[i])
        i++;

    int res = insertrec(node->u.internal.children[i], key, upkey, right);
    if (res <= 0)
        return res;                      /* 重复或下层未分裂，本层无需处理 */

    /* 下层分裂：把提升上来的关键字和新右子树插入本层的第 i 个位置之后 */
    return internalinsert(node, i, *upkey, *right, upkey, right);
}

/* 对外插入接口：处理空树、重复关键字提示以及根分裂长高的情况 */
void insert(BPTree *tree, int key)
{
    /* 空树：新建叶结点直接作为根 */
    if (tree->root == NULL)
    {
        BPtreenode *node = newleaf();
        node->numkeys = 1;
        node->keys[0] = key;
        node->u.leaf.values[0] = key;
        tree->root = node;
        return;
    }

    int upkey = 0;                       /* 分裂时由下层提升上来的关键字 */
    BPtreenode *right = NULL;            /* 分裂产生的右兄弟 */
    int res = insertrec(tree->root, key, &upkey, &right);

    if (res < 0)
    {
        /* 关键字已存在：按题面只提示，不修改树 */
        printf("Key %d is duplicated\n", key);
        return;
    }

    if (res == 1)
    {
        /* 根也发生了分裂：新建根，树长高一层，孩子为原根与新右兄弟 */
        BPtreenode *newroot = newinternal();
        newroot->numkeys = 1;
        newroot->keys[0] = upkey;
        newroot->u.internal.children[0] = tree->root;
        newroot->u.internal.children[1] = right;
        tree->root = newroot;
    }
}

/* 后序递归释放整棵子树的所有结点 */
void free_tree(BPtreenode *node)
{
    if (node == NULL)
        return;
    /* 内部结点：先释放 numkeys + 1 个孩子，再释放自己 */
    if (!node->isleaf)
        for (int i = 0; i <= node->numkeys; i++)
            free_tree(node->u.internal.children[i]);
    free(node);
}

/* 按自顶向下的层次顺序输出整棵树：每层一行，同层结点直接相连，结点形如 [k0,k1,...] */
void print_tree(BPTree *tree)
{
    if (tree->root == NULL)
        return;

    /* level 保存当前层的结点；下一层扫描时按需扩容 */
    int cnt = 1;
    BPtreenode **level = (BPtreenode **)malloc(sizeof(BPtreenode *) * (cnt + 1));
    level[0] = tree->root;

    while (cnt > 0)                       /* 逐层输出，直到没有结点为止 */
    {
        /* 输出本层所有结点 */
        for (int i = 0; i < cnt; i++)
        {
            printf("[");
            for (int j = 0; j < level[i]->numkeys; j++)
            {
                if (j > 0)
                    printf(",");          /* 关键字之间用逗号分隔，无空格 */
                if (level[i]->isleaf)
                    printf("%d", level[i]->u.leaf.values[j]);   /* 叶结点输出数据 */
                else
                    printf("%d", level[i]->keys[j]);            /* 内部结点输出分隔关键字 */
            }
            printf("]");
        }
        printf("\n");

        /* 收集下一层的所有结点（叶结点没有孩子，直接跳过） */
        int nextcap = 16, nextcnt = 0;
        BPtreenode **next = (BPtreenode **)malloc(sizeof(BPtreenode *) * nextcap);
        for (int i = 0; i < cnt; i++)
        {
            if (level[i]->isleaf)
                continue;
            for (int j = 0; j <= level[i]->numkeys; j++)
            {
                if (nextcnt == nextcap)         /* 数组满则容量翻倍 */
                {
                    nextcap *= 2;
                    next = (BPtreenode **)realloc(next, sizeof(BPtreenode *) * nextcap);
                }
                next[nextcnt++] = level[i]->u.internal.children[j];
            }
        }
        free(level);                      /* 释放上一层队列，切换到下一层 */
        level = next;
        cnt = nextcnt;
    }
    free(level);
}

/* 主程序：读入 n 个关键字依次插入空的 3 阶 B+ 树，最后按层输出整棵树 */
int main()
{
    int n;
    struct BPTree *tree = (struct BPTree *)malloc(sizeof(struct BPTree));

    initialize(tree);                     /* 初始化为空树 */

    if (scanf("%d", &n) != 1)
        return 0;                         /* 读不到数据直接结束 */
    for (int i = 0; i < n; i++)
    {
        int key;
        if (scanf("%d", &key) != 1)
            break;
        insert(tree, key);                /* 重复关键字由 insert 内部提示 */
    }

    print_tree(tree);                     /* 按层输出最终结果 */

    free_tree(tree->root);                /* 释放所有结点与树结构本身 */
    free(tree);
    return 0;
}
