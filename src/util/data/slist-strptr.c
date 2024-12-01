#include "slist-strptr.h"
#include "kmalloc.h"
#include "krlibc.h"
#include "klog.h"

slist_sp_t slist_sp_alloc(const char *key, void *val) {
    slist_sp_t node = kmalloc(sizeof(*node));
    if (node == NULL) return NULL;
    node->key  = key ? strdup(key) : NULL;
    node->val  = val;
    node->next = NULL;
    return node;
}

void slist_sp_free(slist_sp_t list) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        kfree(list->key);
        kfree(list);
        list = next;
    }
}

void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *)) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        kfree(list->key);
        free_value(list->val);
        kfree(list);
        list = next;
    }
}

slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *val) {
    slist_sp_t node = slist_sp_alloc(key, val);
    if (node == NULL) return list;

    if (list == NULL) {
        list = node;
    } else {
        slist_sp_t current = list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = node;
    }

    return list;
}

slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *val) {
    slist_sp_t node = slist_sp_alloc(key, val);
    if (node == NULL) return list;

    node->next = list;
    list       = node;

    return list;
}

void *slist_sp_get(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current->val;
    }
    return NULL;
}

slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current;
    }
    return NULL;
}

bool slist_sp_search(slist_sp_t list, void *val, const char **key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->val == val) {
            if (key) *key = current->key;
            return true;
        }
    }
    return false;
}

slist_sp_t slist_sp_search_node(slist_sp_t list, void *val) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->val == val) return current;
    }
    return NULL;
}

slist_sp_t slist_sp_delete(slist_sp_t list, const char *key) {
    if (list == NULL) return NULL;

    if (streq(list->key, key)) {
        slist_sp_t temp = list;
        list            = list->next;
        kfree(temp->key);
        kfree(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (streq(current->key, key)) {
            prev->next = current->next;
            kfree(current->key);
            kfree(current);
            break;
        }
        prev = current;
    }

    return list;
}

slist_sp_t slist_sp_delete_with(slist_sp_t list, const char *key,
                                void (*free_value)(void *)) {
    if (list == NULL) return NULL;

    if (streq(list->key, key)) {
        slist_sp_t temp = list;
        list            = list->next;
        kfree(temp->key);
        free_value(temp->val);
        kfree(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (streq(current->key, key)) {
            prev->next = current->next;
            kfree(current->key);
            free_value(current->val);
            kfree(current);
            break;
        }
        prev = current;
    }

    return list;
}

slist_sp_t slist_sp_delete_node(slist_sp_t list, slist_sp_t node) {
    if (list == NULL || node == NULL) return list;

    if (list == node) {
        slist_sp_t temp = list;
        list            = list->next;
        kfree(temp->key);
        kfree(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (current == node) {
            prev->next = current->next;
            kfree(current->key);
            kfree(current);
            break;
        }
        prev = current;
    }

    return list;
}

slist_sp_t slist_sp_delete_node_with(slist_sp_t list, slist_sp_t node,
                                     void (*free_value)(void *)) {
    if (list == NULL || node == NULL) return list;

    if (list == node) {
        slist_sp_t temp = list;
        list            = list->next;
        kfree(temp->key);
        free_value(temp->val);
        kfree(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (current == node) {
            prev->next = current->next;
            kfree(current->key);
            free_value(current->val);
            kfree(current);
            break;
        }
        prev = current;
    }

    return list;
}

size_t slist_sp_length(slist_sp_t slist_sp) {
    size_t     count   = 0;
    slist_sp_t current = slist_sp;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

void slist_sp_print(slist_sp_t slist_sp) {
    slist_sp_t current = slist_sp;
    while (current != NULL) {
        printk("%p -> ", current->val);
        current = current->next;
    }
    printk("NULL\n");
}

