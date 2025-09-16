#include <kernel/rbtree.h>
#include <kernel/memory/malloc.h>
#include <kernel/string.h>

// 红黑树辅助函数：左旋
static void rbtree_left_rotate(RBTree* tree, RBTNode* x) {
    RBTNode* y = x->right;  // 设置y
    
    // 将y的左子树变为x的右子树
    x->right = y->left;
    if (y->left != tree->nil) {
        y->left->parent = x;
    }
    
    // 将x的父节点设为y的父节点
    y->parent = x->parent;
    
    // 如果x是根节点，则y成为新的根节点
    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->left) {
        // 如果x是其父节点的左子节点，则y成为x父节点的左子节点
        x->parent->left = y;
    } else {
        // 如果x是其父节点的右子节点，则y成为x父节点的右子节点
        x->parent->right = y;
    }
    
    // 将x设为y的左子节点
    y->left = x;
    x->parent = y;
}

// 红黑树辅助函数：右旋
static void rbtree_right_rotate(RBTree* tree, RBTNode* y) {
    RBTNode* x = y->left;  // 设置x
    
    // 将x的右子树变为y的左子树
    y->left = x->right;
    if (x->right != tree->nil) {
        x->right->parent = y;
    }
    
    // 将y的父节点设为x的父节点
    x->parent = y->parent;
    
    // 如果y是根节点，则x成为新的根节点
    if (y->parent == tree->nil) {
        tree->root = x;
    } else if (y == y->parent->left) {
        // 如果y是其父节点的左子节点，则x成为y父节点的左子节点
        y->parent->left = x;
    } else {
        // 如果y是其父节点的右子节点，则x成为y父节点的右子节点
        y->parent->right = x;
    }
    
    // 将y设为x的右子节点
    x->right = y;
    y->parent = x;
}

// 红黑树辅助函数：插入后修复
static void rbtree_insert_fixup(RBTree* tree, RBTNode* z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            RBTNode* y = z->parent->parent->right;
            if (y->color == RB_RED) {
                // 情况1：z的叔叔是红色
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    // 情况2：z的叔叔是黑色，且z是右孩子
                    z = z->parent;
                    rbtree_left_rotate(tree, z);
                }
                // 情况3：z的叔叔是黑色，且z是左孩子
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rbtree_right_rotate(tree, z->parent->parent);
            }
        } else {
            RBTNode* y = z->parent->parent->left;
            if (y->color == RB_RED) {
                // 情况1：z的叔叔是红色（镜像）
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    // 情况2：z的叔叔是黑色，且z是左孩子（镜像）
                    z = z->parent;
                    rbtree_right_rotate(tree, z);
                }
                // 情况3：z的叔叔是黑色，且z是右孩子（镜像）
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rbtree_left_rotate(tree, z->parent->parent);
            }
        }
    }
    // 确保根节点是黑色
    tree->root->color = RB_BLACK;
}

// 红黑树辅助函数：删除后修复
static void rbtree_delete_fixup(RBTree* tree, RBTNode* x) {
    while (x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            RBTNode* w = x->parent->right;
            if (w->color == RB_RED) {
                // 情况1：x的兄弟w是红色
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rbtree_left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                // 情况2：x的兄弟w是黑色，且w的两个孩子都是黑色
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->right->color == RB_BLACK) {
                    // 情况3：x的兄弟w是黑色，w的左孩子是红色，右孩子是黑色
                    w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rbtree_right_rotate(tree, w);
                    w = x->parent->right;
                }
                // 情况4：x的兄弟w是黑色，且w的右孩子是红色
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->right->color = RB_BLACK;
                rbtree_left_rotate(tree, x->parent);
                x = tree->root;  // 退出循环
            }
        } else {
            RBTNode* w = x->parent->left;
            if (w->color == RB_RED) {
                // 情况1：x的兄弟w是红色（镜像）
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rbtree_right_rotate(tree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
                // 情况2：x的兄弟w是黑色，且w的两个孩子都是黑色（镜像）
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->left->color == RB_BLACK) {
                    // 情况3：x的兄弟w是黑色，w的右孩子是红色，左孩子是黑色（镜像）
                    w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rbtree_left_rotate(tree, w);
                    w = x->parent->left;
                }
                // 情况4：x的兄弟w是黑色，且w的左孩子是红色（镜像）
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->left->color = RB_BLACK;
                rbtree_right_rotate(tree, x->parent);
                x = tree->root;  // 退出循环
            }
        }
    }
    x->color = RB_BLACK;
}

// 初始化红黑树
void rbtree_init(RBTree* tree) {
    // 创建哨兵节点
    tree->nil = (RBTNode*)malloc(sizeof(RBTNode));
    tree->nil->color = RB_BLACK;
    tree->nil->left = NULL;
    tree->nil->right = NULL;
    tree->nil->parent = NULL;
    
    // 初始化根节点为哨兵节点
    tree->root = tree->nil;
}

// 在红黑树中搜索指定键值的节点
RBTNode* rbtree_search(RBTree* tree, uint32_t key) {
    RBTNode* current = tree->root;
    
    while (current != tree->nil && key != current->key) {
        if (key < current->key) {
            current = current->left;
        } else {
            current = current->right;
        }
    }
    
    return (current == tree->nil) ? NULL : current;
}

// 插入节点到红黑树
void rbtree_insert(RBTree* tree, RBTNode* z) {
    RBTNode* y = tree->nil;
    RBTNode* x = tree->root;
    
    // 找到插入位置
    while (x != tree->nil) {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        } else {
            x = x->right;
        }
    }
    
    z->parent = y;
    if (y == tree->nil) {
        tree->root = z;  // 树为空，z是根节点
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }
    
    // 初始化z的子节点和颜色
    z->left = tree->nil;
    z->right = tree->nil;
    z->color = RB_RED;
    
    // 修复红黑树性质
    rbtree_insert_fixup(tree, z);
}

// 从红黑树中删除节点
void rbtree_delete(RBTree* tree, RBTNode* z) {
    RBTNode* y = z;
    RBTNode* x;
    RBTColor y_original_color = y->color;
    
    if (z->left == tree->nil) {
        // z没有左孩子，用右孩子替换z
        x = z->right;
        // 用x替换z在树中的位置
        if (z->parent == tree->nil) {
            tree->root = x;
        } else if (z == z->parent->left) {
            z->parent->left = x;
        } else {
            z->parent->right = x;
        }
        x->parent = z->parent;
    } else if (z->right == tree->nil) {
        // z有左孩子但没有右孩子，用左孩子替换z
        x = z->left;
        // 用x替换z在树中的位置
        if (z->parent == tree->nil) {
            tree->root = x;
        } else if (z == z->parent->left) {
            z->parent->left = x;
        } else {
            z->parent->right = x;
        }
        x->parent = z->parent;
    } else {
        // z有两个孩子，找到z的后继y
        y = rbtree_minimum(tree, z->right);
        y_original_color = y->color;
        x = y->right;
        
        if (y->parent == z) {
            x->parent = y;
        } else {
            // 用y的右孩子替换y在树中的位置
            if (y->parent == tree->nil) {
                tree->root = x;
            } else if (y == y->parent->left) {
                y->parent->left = x;
            } else {
                y->parent->right = x;
            }
            x->parent = y->parent;
            
            // 将z的右子树变为y的右子树
            y->right = z->right;
            y->right->parent = y;
        }
        
        // 用y替换z在树中的位置
        if (z->parent == tree->nil) {
            tree->root = y;
        } else if (z == z->parent->left) {
            z->parent->left = y;
        } else {
            z->parent->right = y;
        }
        y->parent = z->parent;
        
        // 将z的左子树变为y的左子树
        y->left = z->left;
        y->left->parent = y;
        
        // 复制z的颜色到y
        y->color = z->color;
    }
    
    // 如果被删除的节点是黑色，需要修复红黑树性质
    if (y_original_color == RB_BLACK) {
        rbtree_delete_fixup(tree, x);
    }
}

// 查找以node为根的子树中的最小节点
RBTNode* rbtree_minimum(RBTree* tree, RBTNode* node) {
    while (node->left != tree->nil) {
        node = node->left;
    }
    return node;
}

// 查找以node为根的子树中的最大节点
RBTNode* rbtree_maximum(RBTree* tree, RBTNode* node) {
    while (node->right != tree->nil) {
        node = node->right;
    }
    return node;
}