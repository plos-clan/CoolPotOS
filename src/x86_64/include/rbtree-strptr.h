#pragma once
#include "alloc.h"
#include "ctype.h"
#include "kprint.h"

#pragma GCC system_header

#ifdef ALL_IMPLEMENTATION
#    define RBTREE_SP_IMPLEMENTATION
#endif

#ifdef RBTREE_SP_IMPLEMENTATION
#    define SLIST_SP_IMPLEMENTATION
#endif
#include "slist-strptr.h"

#ifndef _RBTREE_ENUM_
#    define _RBTREE_ENUM_
enum {
    RBT_RED,  // 红色节点
    RBT_BLACK // 黑色节点
};
#endif

/**
 *\struct rbtree_sp
 *\brief 红黑树节点结构
 */
typedef struct rbtree_sp *rbtree_sp_t;
struct rbtree_sp {
    uint32_t    color;  /**< 节点颜色，取值为 RED 或 BLACK */
    uint32_t    hash;   /**< 节点键哈希值 */
    slist_sp_t  list;   /**< 节点值 */
    rbtree_sp_t left;   /**< 左子节点指针 */
    rbtree_sp_t right;  /**< 右子节点指针 */
    rbtree_sp_t parent; /**< 父节点指针 */
};

#ifdef RBTREE_SP_IMPLEMENTATION
#    define extern static
#endif

/**
 *\brief 创建一个新的红黑树节点
 *\param[in] key 节点键值
 *\param[in] value 节点值指针
 *\return 新的红黑树节点指针
 */
extern rbtree_sp_t rbtree_sp_alloc(const char *key, void *value);

/**
 *\brief 释放红黑树
 *\param[in] root 树的根节点
 */
extern void rbtree_sp_free(rbtree_sp_t root);

extern void rbtree_sp_free_with(rbtree_sp_t root, void (*free_value)(void *));

/**
 *\brief 在红黑树中根据键值查找对应的节点值
 *\param[in] root 树的根节点
 *\param[in] key 要查找的键值
 *\return 若找到对应键值的节点值指针，否则返回NULL
 */
extern void *rbtree_sp_get(rbtree_sp_t root, const char *key);

/**
 *\brief 在红黑树中查找对应值的键值
 *\param[in] root 树的根节点
 *\param[in] value 要查找的值指针
 *\param[out] key 用于存储键值的指针
 *\return 若找到对应值的键值，则返回true，否则返回false
 */
extern bool rbtree_sp_search(rbtree_sp_t root, void *value, const char **key);

/**
 *\brief 在红黑树中插入节点
 *\param[in] root 树的根节点
 *\param[in] key 节点键值
 *\param[in] value 节点值指针
 *\return 插入节点后的树的根节点
 */
extern rbtree_sp_t rbtree_sp_insert(rbtree_sp_t root, const char *key, void *value) __wur;

/**
 *\brief 在红黑树中删除节点
 *\param[in] root 树的根节点
 *\param[in] key 节点键值
 *\return 删除节点后的树的根节点
 */
extern rbtree_sp_t rbtree_sp_delete(rbtree_sp_t root, const char *key) __wur;

/**
 *\brief 按照中序遍历打印红黑树节点
 *\param[in] root 树的根节点
 */
extern void rbtree_sp_print_inorder(rbtree_sp_t root);

/**
 *\brief 按照前序遍历打印红黑树节点
 *\param[in] root 树的根节点
 */
extern void rbtree_sp_print_preorder(rbtree_sp_t root);

/**
 *\brief 按照后序遍历打印红黑树节点
 *\param[in] root 树的根节点
 */
extern void rbtree_sp_print_postorder(rbtree_sp_t root);

#ifdef RBTREE_SP_IMPLEMENTATION
#    undef extern
#endif

#ifdef RBTREE_SP_IMPLEMENTATION

/**
 *\brief 执行左旋操作
 *\param[in] root 树的根节点
 *\param[in] x 要旋转的节点指针
 *\return 旋转后的树的根节点
 */
static rbtree_sp_t rbtree_sp_left_rotate(rbtree_sp_t root, rbtree_sp_t x) __wur;

/**
 *\brief 执行右旋操作
 *\param[in] root 树的根节点
 *\param[in] y 要旋转的节点指针
 *\return 旋转后的树的根节点
 */
static rbtree_sp_t rbtree_sp_right_rotate(rbtree_sp_t root, rbtree_sp_t y) __wur;

/**
 *\brief 替换子树
 *\param[in] root 树的根节点
 *\param[in] u 要替换的子树根节点
 *\param[in] v 替换后的子树根节点
 *\return 替换后的树的根节点
 */
static rbtree_sp_t rbtree_sp_transplant(rbtree_sp_t root, rbtree_sp_t u, rbtree_sp_t v) __wur;

/**
 *\brief 执行插入操作后修复红黑树性质
 *\param[in] root 树的根节点
 *\param[in] z 新插入的节点指针
 *\return 修复后的树的根节点
 */
static rbtree_sp_t rbtree_sp_insert_fixup(rbtree_sp_t root, rbtree_sp_t z) __wur;

/**
 *\brief 执行删除操作后修复红黑树性质
 *\param[in] root 树的根节点
 *\param[in] x 被删除节点的替代节点指针
 *\param[in] x_parent 被删除节点的父节点指针
 *\return 修复后的树的根节点
 */
static rbtree_sp_t rbtree_sp_delete_fixup(rbtree_sp_t root, rbtree_sp_t x, rbtree_sp_t x_parent)
    __wur;

static uint32_t rbtree_sp_hash(const char *str) {
    uint32_t hash = 0;
    if (str)
        for (; *str; str++)
            hash = hash * 131 + *str;
    return hash;
}

static rbtree_sp_t rbtree_sp_alloc(const char *key, void *value) {
    rbtree_sp_t node = malloc(sizeof(*node));
    node->hash       = rbtree_sp_hash(key);
    node->list       = slist_sp_alloc(key, value);
    node->color      = RBT_RED;
    node->left       = NULL;
    node->right      = NULL;
    node->parent     = NULL;
    return node;
}

static void rbtree_sp_free(rbtree_sp_t root) {
    if (root == NULL) return;
    rbtree_sp_free(root->left);
    rbtree_sp_free(root->right);
    slist_sp_free(root->list);
    free(root);
}

static void rbtree_sp_free_with(rbtree_sp_t root, void (*free_value)(void *)) {
    if (root == NULL) return;
    rbtree_sp_free(root->left);
    rbtree_sp_free(root->right);
    slist_sp_free_with(root->list, free_value);
    free(root);
}

static void *rbtree_sp_get(rbtree_sp_t root, const char *key) {
    uint32_t hash = rbtree_sp_hash(key);
    while (root != NULL && root->hash != hash) {
        if (hash < root->hash)
            root = root->left;
        else
            root = root->right;
    }
    return root ? slist_sp_get(root->list, key) : NULL;
}

static rbtree_sp_t rbtree_sp_get_node(rbtree_sp_t root, const char *key) {
    uint32_t hash = rbtree_sp_hash(key);
    while (root != NULL && root->hash != hash) {
        if (hash < root->hash)
            root = root->left;
        else
            root = root->right;
    }
    return root;
}

static bool rbtree_sp_search(rbtree_sp_t root, void *value, const char **key) {
    if (root == NULL) return false;
    if (slist_sp_search(root->list, value, key)) return true;
    if (root->left && rbtree_sp_search(root->left, value, key)) return true;
    if (root->right && rbtree_sp_search(root->right, value, key)) return true;
    return false;
}

static rbtree_sp_t rbtree_sp_min(rbtree_sp_t root) {
    while (root->left != NULL)
        root = root->left;
    return root;
}

static rbtree_sp_t rbtree_sp_max(rbtree_sp_t root) {
    while (root->right != NULL)
        root = root->right;
    return root;
}

static rbtree_sp_t rbtree_sp_left_rotate(rbtree_sp_t root, rbtree_sp_t x) {
    rbtree_sp_t y = x->right;
    x->right      = y->left;

    if (y->left != NULL) y->left->parent = x;

    y->parent = x->parent;

    if (x->parent == NULL)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left   = x;
    x->parent = y;

    return root;
}

static rbtree_sp_t rbtree_sp_right_rotate(rbtree_sp_t root, rbtree_sp_t y) {
    rbtree_sp_t x = y->left;
    y->left       = x->right;

    if (x->right != NULL) x->right->parent = y;

    x->parent = y->parent;

    if (y->parent == NULL)
        root = x;
    else if (y == y->parent->left)
        y->parent->left = x;
    else
        y->parent->right = x;

    x->right  = y;
    y->parent = x;

    return root;
}

static rbtree_sp_t rbtree_sp_transplant(rbtree_sp_t root, rbtree_sp_t u, rbtree_sp_t v) {
    if (u->parent == NULL)
        root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;

    if (v != NULL) v->parent = u->parent;

    return root;
}

static rbtree_sp_t rbtree_sp_insert_fixup(rbtree_sp_t root, rbtree_sp_t z) {
    while (z != root && z->parent->color == RBT_RED) {
        if (z->parent == z->parent->parent->left) {
            rbtree_sp_t y = z->parent->parent->right;
            if (y != NULL && y->color == RBT_RED) {
                z->parent->color         = RBT_BLACK;
                y->color                 = RBT_BLACK;
                z->parent->parent->color = RBT_RED;
                z                        = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z    = z->parent;
                    root = rbtree_sp_left_rotate(root, z);
                }
                z->parent->color         = RBT_BLACK;
                z->parent->parent->color = RBT_RED;
                root                     = rbtree_sp_right_rotate(root, z->parent->parent);
            }
        } else {
            rbtree_sp_t y = z->parent->parent->left;
            if (y != NULL && y->color == RBT_RED) {
                z->parent->color         = RBT_BLACK;
                y->color                 = RBT_BLACK;
                z->parent->parent->color = RBT_RED;
                z                        = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z    = z->parent;
                    root = rbtree_sp_right_rotate(root, z);
                }
                z->parent->color         = RBT_BLACK;
                z->parent->parent->color = RBT_RED;
                root                     = rbtree_sp_left_rotate(root, z->parent->parent);
            }
        }
    }

    root->color = RBT_BLACK;
    return root;
}

static rbtree_sp_t rbtree_sp_insert(rbtree_sp_t root, const char *key, void *value) {

    rbtree_sp_t node = rbtree_sp_get_node(root, key);
    if (node) {
        slist_sp_prepend(node->list, key, value);
        return root;
    }

    uint32_t hash = rbtree_sp_hash(key);

    rbtree_sp_t z = rbtree_sp_alloc(key, value);

    rbtree_sp_t y = NULL;
    rbtree_sp_t x = root;

    while (x != NULL) {
        y = x;
        if (hash < x->hash)
            x = x->left;
        else
            x = x->right;
    }

    z->parent = y;
    if (y == NULL)
        root = z;
    else if (hash < y->hash)
        y->left = z;
    else
        y->right = z;

    return rbtree_sp_insert_fixup(root, z);
}

static rbtree_sp_t rbtree_sp_delete_fixup(rbtree_sp_t root, rbtree_sp_t x, rbtree_sp_t x_parent) {
    while (x != root && (x == NULL || x->color == RBT_BLACK)) {
        if (x == x_parent->left) {
            rbtree_sp_t w = x_parent->right;
            if (w->color == RBT_RED) {
                w->color        = RBT_BLACK;
                x_parent->color = RBT_RED;
                root            = rbtree_sp_left_rotate(root, x_parent);
                w               = x_parent->right;
            }
            if ((w->left == NULL || w->left->color == RBT_BLACK) &&
                (w->right == NULL || w->right->color == RBT_BLACK)) {
                w->color = RBT_RED;
                x        = x_parent;
                x_parent = x_parent->parent;
            } else {
                if (w->right == NULL || w->right->color == RBT_BLACK) {
                    if (w->left != NULL) w->left->color = RBT_BLACK;
                    w->color = RBT_RED;
                    root     = rbtree_sp_right_rotate(root, w);
                    w        = x_parent->right;
                }
                w->color        = x_parent->color;
                x_parent->color = RBT_BLACK;
                if (w->right != NULL) w->right->color = RBT_BLACK;
                root = rbtree_sp_left_rotate(root, x_parent);
                x    = root;
            }
        } else {
            rbtree_sp_t w = x_parent->left;
            if (w->color == RBT_RED) {
                w->color        = RBT_BLACK;
                x_parent->color = RBT_RED;
                root            = rbtree_sp_right_rotate(root, x_parent);
                w               = x_parent->left;
            }
            if ((w->right == NULL || w->right->color == RBT_BLACK) &&
                (w->left == NULL || w->left->color == RBT_BLACK)) {
                w->color = RBT_RED;
                x        = x_parent;
                x_parent = x_parent->parent;
            } else {
                if (w->left == NULL || w->left->color == RBT_BLACK) {
                    if (w->right != NULL) w->right->color = RBT_BLACK;
                    w->color = RBT_RED;
                    root     = rbtree_sp_left_rotate(root, w);
                    w        = x_parent->left;
                }
                w->color        = x_parent->color;
                x_parent->color = RBT_BLACK;
                if (w->left != NULL) w->left->color = RBT_BLACK;
                root = rbtree_sp_right_rotate(root, x_parent);
                x    = root;
            }
        }
    }

    if (x != NULL) x->color = RBT_BLACK;
    return root;
}

static rbtree_sp_t rbtree_sp_delete(rbtree_sp_t root, const char *key) {
    if (root == NULL) return NULL;
    rbtree_sp_t z = rbtree_sp_get_node(root, key);
    if (z == NULL) return root;
    if (slist_sp_delete(z->list, key) != NULL) return root;

    rbtree_sp_t x;
    rbtree_sp_t x_parent;
    int32_t     original_color = z->color;

    if (z->left == NULL) {
        x        = z->right;
        x_parent = z->parent;
        root     = rbtree_sp_transplant(root, z, z->right);
    } else if (z->right == NULL) {
        x        = z->left;
        x_parent = z->parent;
        root     = rbtree_sp_transplant(root, z, z->left);
    } else {
        rbtree_sp_t y  = rbtree_sp_min(z->right);
        original_color = y->color;
        x              = y->right;
        x_parent       = y;
        if (y->parent == z) {
            if (x != NULL) x->parent = y;
        } else {
            root     = rbtree_sp_transplant(root, y, y->right);
            y->right = z->right;
            if (y->right != NULL) y->right->parent = y;
        }
        root            = rbtree_sp_transplant(root, z, y);
        y->left         = z->left;
        y->left->parent = y;
        y->color        = z->color;
    }

    free(z);

    if (original_color == RBT_BLACK) root = rbtree_sp_delete_fixup(root, x, x_parent);

    return root;
}

static void _rbtree_sp_print_inorder(rbtree_sp_t root, int deepth) {
    if (root == NULL) return;
    _rbtree_sp_print_inorder(root->left, deepth + 1);
    for (int i = 0; i < deepth; i++)
        printk("| ");
    printk("%d => ", root->hash);
    slist_sp_print(root->list);
    _rbtree_sp_print_inorder(root->right, deepth + 1);
}
static void rbtree_sp_print_inorder(rbtree_sp_t root) {
    printk("In-order traversal of the Red-Black Tree: \n");
    _rbtree_sp_print_inorder(root, 0);
}

static void _rbtree_sp_print_preorder(rbtree_sp_t root, int deepth) {
    if (root == NULL) return;
    for (int i = 0; i < deepth; i++)
        printk("| ");
    printk("%d => ", root->hash);
    slist_sp_print(root->list);
    _rbtree_sp_print_preorder(root->left, deepth + 1);
    _rbtree_sp_print_preorder(root->right, deepth + 1);
}
static void rbtree_sp_print_preorder(rbtree_sp_t root) {
    printk("Pre-order traversal of the Red-Black Tree: \n");
    _rbtree_sp_print_preorder(root, 0);
}

static void _rbtree_sp_print_postorder(rbtree_sp_t root, int deepth) {
    if (root == NULL) return;
    _rbtree_sp_print_postorder(root->left, deepth + 1);
    _rbtree_sp_print_postorder(root->right, deepth + 1);
    for (int i = 0; i < deepth; i++)
        printk("| ");
    printk("%d => ", root->hash);
    slist_sp_print(root->list);
}
static void rbtree_sp_print_postorder(rbtree_sp_t root) {
    printk("Post-order traversal of the Red-Black Tree: \n");
    _rbtree_sp_print_postorder(root, 0);
}

#    undef RBTREE_SP_IMPLEMENTATION
#endif

/**
 *\brief 在红黑树中插入节点
 *\param[in,out] root 树的根节点
 *\param[in] key 节点键值
 *\param[in] value 节点值指针
 */
#define rbtree_sp_insert(root, key, value) ((root) = rbtree_sp_insert(root, key, value))

/**
 *\brief 在红黑树中删除节点
 *\param[in,out] root 树的根节点
 *\param[in] key 节点键值
 */
#define rbtree_sp_delete(root, key) ((root) = rbtree_sp_delete(root, key))
