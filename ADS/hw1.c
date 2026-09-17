#include<stdio.h>
#include<stdlib.h>
int max(int a,int b)
{
    return a>b?a:b;
}
struct node
{
    int data;
    int height;
    struct node *left;
    struct node *right;
};
typedef struct node* Node;
int getHeight(Node head)
{
    if(head == NULL)
        return 0;
    return head->height;
}

void updateHeight(Node head)
{
    if(head == NULL)
        return;
    head->height = max(getHeight(head->left), getHeight(head->right)) + 1;
}

void rotateRight(Node *head)
{
    Node newHead = (*head)->left;
    (*head)->left = newHead->right;
    newHead->right = *head;
    *head = newHead;
    updateHeight((*head)->right);
    updateHeight(*head);
}
void rotateLeft(Node *head)
{
    Node newHead = (*head)->right;
    (*head)->right = newHead->left;
    newHead->left = *head;
    *head = newHead;
    updateHeight((*head)->left);
    updateHeight(*head);
}

void insert(Node *head, int data)
{
    if (*head == NULL)
    {
        Node newNode = (Node)malloc(sizeof(struct node));
        newNode->data = data;
        newNode->left = NULL;
        newNode->right = NULL;
        newNode->height = 1;
        *head = newNode;
        return;
    }
    // 插入并计算height
    if(data < (*head)->data)
    {
        insert(&((*head)->left), data);
    }
    else
    {
        insert(&((*head)->right), data);
    }
    updateHeight(*head);
    if(getHeight((*head)->left) - getHeight((*head)->right) > 1)   // 左子树高于右子树
    {
        if(data < (*head)->left->data)   // LL型
        {
            rotateRight(head);
        }
        else    // LR型
        {
            rotateLeft(&((*head)->left));
            rotateRight(head);
        }
    }
    else if(getHeight((*head)->right) - getHeight((*head)->left) > 1)  // 右子树高于左子树
    {
        if(data > (*head)->right->data)  // RR型
        {
            rotateLeft(head);
        }
        else    // RL型
        {
            rotateRight(&((*head)->right));
            rotateLeft(head);
        }
    }
}



int main()
{
    int n;
    scanf("%d",&n);
    Node head = NULL;
    for(int i=0;i<n;i++)
    {
        int data;
        scanf("%d",&data);
        insert(&head,data);
    }
    printf("%d",head->data);

    return 0;
}

//这一版在处理相同数据时会导致程序崩溃。因为在插入相同数据时，程序会一直向右子树插入，最终导致无限递归。
//修改方案:比较孙节点平衡因子。