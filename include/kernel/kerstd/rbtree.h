#ifndef OS_KERNEL_RBTREE_H
#define OS_KERNEL_RBTREE_H

#include <stdtype.h>

// 红黑树节点颜色枚举
typedef enum {
    RB_BLACK,
    RB_RED
} RBTColor;

// 红黑树节点结构
typedef struct RBTNode {
    uint32_t key;                   // 键值，用于系统调用时是系统调用号
    void* data;                     // 指向节点关联的数据
    RBTColor color;                 // 节点颜色
    struct RBTNode* parent;         // 父节点
    struct RBTNode* left;           // 左子节点
    struct RBTNode* right;          // 右子节点
} RBTNode;

// 红黑树结构
typedef struct {
    RBTNode* root;                  // 根节点
    RBTNode* nil;                   // 哨兵节点，代表NULL
} RBTree;

// 函数声明
void rbtree_init(RBTree* tree);
RBTNode* rbtree_search(RBTree* tree, uint32_t key);
void rbtree_insert(RBTree* tree, RBTNode* node);
void rbtree_delete(RBTree* tree, RBTNode* node);
RBTNode* rbtree_minimum(RBTree* tree, RBTNode* node);
RBTNode* rbtree_maximum(RBTree* tree, RBTNode* node);

#endif