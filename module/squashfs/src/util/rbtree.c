#include "config.h"
#include "sqfs/error.h"
#include "util/rbtree.h"

#define IS_RED(n) ((n) && (n)->is_red)

static void destroy_nodes_dfs(rbtree_node_t *n) {
    rbtree_node_t *left, *right;

    if (n == NULL)
        return;

    left = n->left;
    right = n->right;
    free(n);
    destroy_nodes_dfs(left);
    destroy_nodes_dfs(right);
}

static void flip_colors(rbtree_node_t *n) {
    n->is_red = !n->is_red;
    n->left->is_red = !n->left->is_red;
    n->right->is_red = !n->right->is_red;
}

static rbtree_node_t *rotate_right(rbtree_node_t *n) {
    rbtree_node_t *x = n->left;
    n->left = x->right;
    x->right = n;
    x->is_red = x->right->is_red;
    x->right->is_red = 1;
    return x;
}

static rbtree_node_t *rotate_left(rbtree_node_t *n) {
    rbtree_node_t *x = n->right;
    n->right = x->left;
    x->left = n;
    x->is_red = x->left->is_red;
    x->left->is_red = 1;
    return x;
}

static rbtree_node_t *subtree_balance(rbtree_node_t *n) {
    if (IS_RED(n->right) && !IS_RED(n->left))
        n = rotate_left(n);

    if (IS_RED(n->left) && IS_RED(n->left->left))
        n = rotate_right(n);

    if (IS_RED(n->left) && IS_RED(n->right))
        flip_colors(n);

    return n;
}

static rbtree_node_t *subtree_insert(rbtree_t *tree, rbtree_node_t *root, rbtree_node_t *new_node) {
    if (root == NULL)
        return new_node;

    if (tree->key_compare(tree->key_context, new_node->data, root->data) < 0) {
        root->left = subtree_insert(tree, root->left, new_node);
    } else {
        root->right = subtree_insert(tree, root->right, new_node);
    }

    return subtree_balance(root);
}

static rbtree_node_t *mknode(rbtree_t *tree, const void *key, const void *value) {
    rbtree_node_t *node = calloc(1, sizeof(*node) + tree->key_size_padded + tree->value_size);
    if (node == NULL)
        return NULL;

    node->value_offset = tree->key_size_padded;
    node->is_red = 1;
    memcpy(node->data, key, tree->key_size);
    memcpy(node->data + tree->key_size_padded, value, tree->value_size);
    return node;
}

static rbtree_node_t *copy_node(const rbtree_t *src_tree, const rbtree_node_t *node) {
    rbtree_node_t *out = calloc(1, sizeof(*out) + src_tree->key_size_padded + src_tree->value_size);
    if (out == NULL)
        return NULL;

    memcpy(out, node, sizeof(*node) + src_tree->key_size_padded + src_tree->value_size);
    out->left = NULL;
    out->right = NULL;

    if (node->left != NULL) {
        out->left = copy_node(src_tree, node->left);
        if (out->left == NULL) {
            destroy_nodes_dfs(out);
            return NULL;
        }
    }

    if (node->right != NULL) {
        out->right = copy_node(src_tree, node->right);
        if (out->right == NULL) {
            destroy_nodes_dfs(out);
            return NULL;
        }
    }

    return out;
}

int rbtree_init(rbtree_t *tree, size_t keysize, size_t valuesize,
                int (*key_compare)(const void *, const void *, const void *)) {
    size_t diff, size;

    memset(tree, 0, sizeof(*tree));
    tree->key_compare = key_compare;
    tree->key_size = keysize;
    tree->key_size_padded = keysize;
    tree->value_size = valuesize;

    diff = keysize % sizeof(void *);
    if (diff != 0) {
        diff = sizeof(void *) - diff;
        if (SZ_ADD_OV(tree->key_size_padded, diff, &tree->key_size_padded))
            return SQFS_ERROR_OVERFLOW;
    }

    if (sizeof(size_t) > sizeof(sqfs_u32) && tree->key_size_padded > 0x0FFFFFFFFUL)
        return SQFS_ERROR_OVERFLOW;

    size = sizeof(rbtree_node_t);
    if (SZ_ADD_OV(size, tree->key_size_padded, &size))
        return SQFS_ERROR_OVERFLOW;
    if (SZ_ADD_OV(size, tree->value_size, &size))
        return SQFS_ERROR_OVERFLOW;

    return 0;
}

int rbtree_copy(const rbtree_t *tree, rbtree_t *out) {
    memcpy(out, tree, sizeof(*out));
    out->root = NULL;

    if (tree->root != NULL) {
        out->root = copy_node(tree, tree->root);
        if (out->root == NULL) {
            memset(out, 0, sizeof(*out));
            return SQFS_ERROR_ALLOC;
        }
    }

    return 0;
}

void rbtree_cleanup(rbtree_t *tree) {
    destroy_nodes_dfs(tree->root);
    memset(tree, 0, sizeof(*tree));
}

int rbtree_insert(rbtree_t *tree, const void *key, const void *value) {
    rbtree_node_t *node = mknode(tree, key, value);
    if (node == NULL)
        return SQFS_ERROR_ALLOC;

    tree->root = subtree_insert(tree, tree->root, node);
    tree->root->is_red = 0;
    return 0;
}

rbtree_node_t *rbtree_lookup(const rbtree_t *tree, const void *key) {
    rbtree_node_t *node = tree->root;

    while (node != NULL) {
        int ret = tree->key_compare(tree->key_context, key, node->data);
        if (ret == 0)
            break;

        node = ret < 0 ? node->left : node->right;
    }

    return node;
}
