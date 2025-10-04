#pragma once

#include "cptype.h"

#pragma GCC system_header

#ifdef ALL_IMPLEMENTATION
#    define SLIST_SP_IMPLEMENTATION
#endif

/**
 *\struct ListNode
 *\brief 单向链表节点结构
 */
typedef struct slist_sp *slist_sp_t;
struct slist_sp {
    char      *key;
    void      *val;
    slist_sp_t next;
};

#ifdef SLIST_SP_IMPLEMENTATION
#    define extern static
#endif

/**
 *\brief 创建一个新的带字符串键的单向链表节点
 *\param[in] key 节点键值（字符串）
 *\param[in] val 节点值指针
 *\return 新的单向链表节点指针
 */
extern slist_sp_t slist_sp_alloc(const char *key, void *val) __THROW;

/**
 *\brief 释放带字符串键的单向链表
 *\param[in] list 单向链表头指针
 */
extern void slist_sp_free(slist_sp_t list) __THROW;

extern void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *)) __THROW;

/**
 *\brief 在带字符串键的单向链表末尾插入节点
 *\param[in] list 单向链表头指针
 *\param[in] key 节点键值（字符串）
 *\param[in] val 节点值指针
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *val) __THROW;

/**
 *\brief 在带字符串键的单向链表开头插入节点
 *\param[in] list 单向链表头指针
 *\param[in] key 节点键值（字符串）
 *\param[in] val 节点值指针
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *val) __THROW;

/**
 *\brief 在带字符串键的单向链表中根据键值查找对应的节点值
 *\param[in] list 单向链表头指针
 *\param[in] key 要查找的键值（字符串）
 *\return 若找到对应键值的节点值指针，否则返回NULL
 */
extern void *slist_sp_get(slist_sp_t list, const char *key) __THROW;

/**
 *\brief 在带字符串键的单向链表中根据键值查找对应的节点
 *\param[in] list 单向链表头指针
 *\param[in] key 要查找的键值（字符串）
 *\return 若找到对应键值的节点指针，否则返回NULL
 */
extern slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key) __THROW;

/**
 *\brief 在带字符串键的单向链表中查找对应值的键值
 *\param[in] list 单向链表头指针
 *\param[in] val 要查找的值指针
 *\param[out] key 用于存储键值的指针
 *\return 若找到对应值的键值，则返回true，否则返回false
 */
extern bool slist_sp_search(slist_sp_t list, void *val, const char **key) __THROW;

/**
 *\brief 在带字符串键的单向链表中查找对应值的节点
 *\param[in] list 单向链表头指针
 *\param[in] val 要查找的值指针
 *\return 若找到对应值的节点指针，否则返回NULL
 */
extern slist_sp_t slist_sp_search_node(slist_sp_t list, void *val) __THROW;

/**
 *\brief 在带字符串键的单向链表中根据键值删除节点
 *\param[in] list 单向链表头指针
 *\param[in] key 要删除的节点键值（字符串）
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_delete(slist_sp_t list, const char *key) __THROW;

extern slist_sp_t slist_sp_delete_with(slist_sp_t list, const char *key, void (*free_value)(void *))
    __THROW;

/**
 *\brief 在带字符串键的单向链表中删除指定节点
 *\param[in] list 单向链表头指针
 *\param[in] node 要删除的节点
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_delete_node(slist_sp_t list, slist_sp_t node) __THROW;

extern slist_sp_t slist_sp_delete_node_with(slist_sp_t list, slist_sp_t node,
                                            void (*free_value)(void *)) __THROW;

/**
 *\brief 带字符串键的单向链表的长度
 *\param[in] list 单向链表头指针
 *\return 单向链表的长度
 */
extern size_t slist_sp_length(slist_sp_t list) __THROW;

/**
 *\brief 打印带字符串键的单向链表中的节点数据
 *\param[in] list 单向链表头指针
 */
extern void slist_sp_print(slist_sp_t list) __THROW;

#ifdef SLIST_SP_IMPLEMENTATION
#    undef extern
#endif

#ifdef SLIST_SP_IMPLEMENTATION

static slist_sp_t slist_sp_alloc(const char *key, void *val) {
    slist_sp_t node = kmalloc(sizeof(*node));
    if (node == NULL) return NULL;
    node->key  = key ? strdup(key) : NULL;
    node->val  = val;
    node->next = NULL;
    return node;
}

static void slist_sp_free(slist_sp_t list) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        kfree(list->key);
        kfree(list);
        list = next;
    }
}

static void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *)) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        kfree(list->key);
        free_value(list->val);
        kfree(list);
        list = next;
    }
}

static slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *val) {
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

static slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *val) {
    slist_sp_t node = slist_sp_alloc(key, val);
    if (node == NULL) return list;

    node->next = list;
    list       = node;

    return list;
}

static void *slist_sp_get(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current->val;
    }
    return NULL;
}

static slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current;
    }
    return NULL;
}

static bool slist_sp_search(slist_sp_t list, void *val, const char **key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->val == val) {
            if (key) *key = current->key;
            return true;
        }
    }
    return false;
}

static slist_sp_t slist_sp_search_node(slist_sp_t list, void *val) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->val == val) return current;
    }
    return NULL;
}

static slist_sp_t slist_sp_delete(slist_sp_t list, const char *key) {
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

static slist_sp_t slist_sp_delete_with(slist_sp_t list, const char *key,
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

static slist_sp_t slist_sp_delete_node(slist_sp_t list, slist_sp_t node) {
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

static slist_sp_t slist_sp_delete_node_with(slist_sp_t list, slist_sp_t node,
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

static size_t slist_sp_length(slist_sp_t slist_sp) {
    size_t     count   = 0;
    slist_sp_t current = slist_sp;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

static void slist_sp_print(slist_sp_t slist_sp) {
    slist_sp_t current = slist_sp;
    while (current != NULL) {
        printk("%p -> ", current->val);
        current = current->next;
    }
    printk("NULL\n");
}

#    undef SLIST_SP_IMPLEMENTATION
#endif

/**
 *\brief 在单向链表末尾插入节点
 *\param[in,out] list 单向链表头指针
 *\param[in] val 节点值
 */
#define slist_sp_append(list, key, val) (list) = (slist_sp_append(list, key, val))

/**
 *\brief 在单向链表开头插入节点
 *\param[in,out] list 单向链表头指针
 *\param[in] val 节点值
 */
#define slist_sp_prepend(list, key, val) ((list) = slist_sp_prepend(list, key, val))

/**
 *\brief 删除单向链表中的节点
 *\param[in,out] list 单向链表头指针
 *\param[in] key 要删除的节点键
 */
#define slist_sp_delete(list, key) ((list) = slist_sp_delete(list, key))

/**
 *\brief 遍历单向链表中的节点并执行操作
 *\param[in] list 单向链表头指针
 *\param[in] node 用于迭代的节点指针变量
 */
#define slist_sp_foreach(list, node) for (slist_sp_t node = (list); node; node = node->next)