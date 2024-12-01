#include "rbtree-strptr.h"
#include "kmalloc.h"
#include "klog.h"

uint32_t rbtree_sp_hash(const char *str) {
    uint32_t hash = 0;
    if (str)
        for (; *str; str++)
            hash = hash * 131 + *str;
    return hash;
}

rbtree_sp_t rbtree_sp_alloc(const char *key, void *value) {
    rbtree_sp_t node = kmalloc(sizeof(*node));
    node->hash       = rbtree_sp_hash(key);
    node->list       = slist_sp_alloc(key, value);
    node->color      = RBT_RED;
    node->left       = NULL;
    node->right      = NULL;
    node->parent     = NULL;
    return node;
}

void rbtree_sp_free(rbtree_sp_t root) {
    if (root == NULL) return;
    rbtree_sp_free(root->left);
    rbtree_sp_free(root->right);
    slist_sp_free(root->list);
    kfree(root);
}

void rbtree_sp_free_with(rbtree_sp_t root, void (*free_value)(void *)) {
    if (root == NULL) return;
    rbtree_sp_free(root->left);
    rbtree_sp_free(root->right);
    slist_sp_free_with(root->list, free_value);
    kfree(root);
}

void *rbtree_sp_get(rbtree_sp_t root, const char *key) {
    uint32_t hash = rbtree_sp_hash(key);
    while (root != NULL && root->hash != hash) {
        if (hash < root->hash)
            root = root->left;
        else
            root = root->right;
    }
    return root ? slist_sp_get(root->list, key) : NULL;
}

rbtree_sp_t rbtree_sp_get_node(rbtree_sp_t root, const char *key) {
    uint32_t hash = rbtree_sp_hash(key);
    while (root != NULL && root->hash != hash) {
        if (hash < root->hash)
            root = root->left;
        else
            root = root->right;
    }
    return root;
}

bool rbtree_sp_search(rbtree_sp_t root, void *value, const char **key) {
    if (root == NULL) return false;
    if (slist_sp_search(root->list, value, key)) return true;
    if (root->left && rbtree_sp_search(root->left, value, key)) return true;
    if (root->right && rbtree_sp_search(root->right, value, key)) return true;
    return false;
}

rbtree_sp_t rbtree_sp_min(rbtree_sp_t root) {
    while (root->left != NULL)
        root = root->left;
    return root;
}

rbtree_sp_t rbtree_sp_max(rbtree_sp_t root) {
    while (root->right != NULL)
        root = root->right;
    return root;
}

rbtree_sp_t rbtree_sp_left_rotate(rbtree_sp_t root, rbtree_sp_t x) {
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

rbtree_sp_t rbtree_sp_right_rotate(rbtree_sp_t root, rbtree_sp_t y) {
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

rbtree_sp_t rbtree_sp_transplant(rbtree_sp_t root, rbtree_sp_t u, rbtree_sp_t v) {
    if (u->parent == NULL)
        root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;

    if (v != NULL) v->parent = u->parent;

    return root;
}

rbtree_sp_t rbtree_sp_insert_fixup(rbtree_sp_t root, rbtree_sp_t z) {
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

rbtree_sp_t rbtree_sp_insert(rbtree_sp_t root, const char *key, void *value) {

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

rbtree_sp_t rbtree_sp_delete_fixup(rbtree_sp_t root, rbtree_sp_t x, rbtree_sp_t x_parent) {
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

rbtree_sp_t rbtree_sp_delete(rbtree_sp_t root, const char *key) {
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

    kfree(z);

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

void rbtree_sp_print_inorder(rbtree_sp_t root) {
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

void rbtree_sp_print_preorder(rbtree_sp_t root) {
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

void rbtree_sp_print_postorder(rbtree_sp_t root) {
    printk("Post-order traversal of the Red-Black Tree: \n");
    _rbtree_sp_print_postorder(root, 0);
}