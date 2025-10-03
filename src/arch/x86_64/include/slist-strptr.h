#pragma once
#include "ctype.h"
#include "kprint.h"
#include "krlibc.h"

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
    void      *value;
    slist_sp_t next;
};

#ifdef SLIST_SP_IMPLEMENTATION
#    define extern static
#endif

/**
 *\brief 创建一个新的带字符串键的单向链表节点
 *\param[in] key 节点键值（字符串）
 *\param[in] value 节点值指针
 *\return 新的单向链表节点指针
 */
extern slist_sp_t slist_sp_alloc(const char *key, void *value);

/**
 *\brief 释放带字符串键的单向链表
 *\param[in] list 单向链表头指针
 */
extern void slist_sp_free(slist_sp_t list);

extern void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *));

/**
 *\brief 在带字符串键的单向链表末尾插入节点
 *\param[in] list 单向链表头指针
 *\param[in] key 节点键值（字符串）
 *\param[in] value 节点值指针
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *value);

/**
 *\brief 在带字符串键的单向链表开头插入节点
 *\param[in] list 单向链表头指针
 *\param[in] key 节点键值（字符串）
 *\param[in] value 节点值指针
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *value);

/**
 *\brief 在带字符串键的单向链表中根据键值查找对应的节点值
 *\param[in] list 单向链表头指针
 *\param[in] key 要查找的键值（字符串）
 *\return 若找到对应键值的节点值指针，否则返回NULL
 */
extern void *slist_sp_get(slist_sp_t list, const char *key);

/**
 *\brief 在带字符串键的单向链表中根据键值查找对应的节点
 *\param[in] list 单向链表头指针
 *\param[in] key 要查找的键值（字符串）
 *\return 若找到对应键值的节点指针，否则返回NULL
 */
extern slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key);

/**
 *\brief 在带字符串键的单向链表中查找对应值的键值
 *\param[in] list 单向链表头指针
 *\param[in] value 要查找的值指针
 *\param[out] key 用于存储键值的指针
 *\return 若找到对应值的键值，则返回true，否则返回false
 */
extern bool slist_sp_search(slist_sp_t list, void *value, const char **key);

/**
 *\brief 在带字符串键的单向链表中查找对应值的节点
 *\param[in] list 单向链表头指针
 *\param[in] value 要查找的值指针
 *\return 若找到对应值的节点指针，否则返回NULL
 */
extern slist_sp_t slist_sp_search_node(slist_sp_t list, void *value);

/**
 *\brief 在带字符串键的单向链表中根据键值删除节点
 *\param[in] list 单向链表头指针
 *\param[in] key 要删除的节点键值（字符串）
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_delete(slist_sp_t list, const char *key);

extern slist_sp_t slist_sp_delete_with(slist_sp_t list, const char *key,
                                       void (*free_value)(void *));

/**
 *\brief 在带字符串键的单向链表中删除指定节点
 *\param[in] list 单向链表头指针
 *\param[in] node 要删除的节点
 *\return 更新后的单向链表头指针
 */
extern slist_sp_t slist_sp_delete_node(slist_sp_t list, slist_sp_t node);

extern slist_sp_t slist_sp_delete_node_with(slist_sp_t list, slist_sp_t node,
                                            void (*free_value)(void *));

/**
 *\brief 带字符串键的单向链表的长度
 *\param[in] list 单向链表头指针
 *\return 单向链表的长度
 */
extern size_t slist_sp_length(slist_sp_t list);

/**
 *\brief 打印带字符串键的单向链表中的节点数据
 *\param[in] list 单向链表头指针
 */
extern void slist_sp_print(slist_sp_t list);

#ifdef SLIST_SP_IMPLEMENTATION
#    undef extern
#endif

#ifdef SLIST_SP_IMPLEMENTATION

static slist_sp_t slist_sp_alloc(const char *key, void *value) {
    slist_sp_t node = malloc(sizeof(*node));
    if (node == NULL) return NULL;
    node->key   = key ? strdup(key) : NULL;
    node->value = value;
    node->next  = NULL;
    return node;
}

static void slist_sp_free(slist_sp_t list) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        free(list->key);
        free(list);
        list = next;
    }
}

static void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *)) {
    while (list != NULL) {
        slist_sp_t next = list->next;
        free(list->key);
        free_value(list->value);
        free(list);
        list = next;
    }
}

static slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *value) {
    slist_sp_t node = slist_sp_alloc(key, value);
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

static slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *value) {
    slist_sp_t node = slist_sp_alloc(key, value);
    if (node == NULL) return list;

    node->next = list;
    list       = node;

    return list;
}

static void *slist_sp_get(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current->value;
    }
    return NULL;
}

static slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (streq(current->key, key)) return current;
    }
    return NULL;
}

static bool slist_sp_search(slist_sp_t list, void *value, const char **key) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->value == value) {
            if (key) *key = current->key;
            return true;
        }
    }
    return false;
}

static slist_sp_t slist_sp_search_node(slist_sp_t list, void *value) {
    for (slist_sp_t current = list; current; current = current->next) {
        if (current->value == value) return current;
    }
    return NULL;
}

static slist_sp_t slist_sp_delete(slist_sp_t list, const char *key) {
    if (list == NULL) return NULL;

    if (streq(list->key, key)) {
        slist_sp_t temp = list;
        list            = list->next;
        free(temp->key);
        free(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (streq(current->key, key)) {
            prev->next = current->next;
            free(current->key);
            free(current);
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
        free(temp->key);
        free_value(temp->value);
        free(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (streq(current->key, key)) {
            prev->next = current->next;
            free(current->key);
            free_value(current->value);
            free(current);
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
        free(temp->key);
        free(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (current == node) {
            prev->next = current->next;
            free(current->key);
            free(current);
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
        free(temp->key);
        free_value(temp->value);
        free(temp);
        return list;
    }

    slist_sp_t prev = list;
    for (slist_sp_t current = list->next; current != NULL; current = current->next) {
        if (current == node) {
            prev->next = current->next;
            free(current->key);
            free_value(current->value);
            free(current);
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
        printk("%p -> ", current->value);
        current = current->next;
    }
    printk("NULL\n");
}

#    undef SLIST_SP_IMPLEMENTATION
#endif

/**
 *\brief 在单向链表末尾插入节点
 *\param[in,out] list 单向链表头指针
 *\param[in] value 节点值
 */
#define slist_sp_append(list, key, value) (list) = (slist_sp_append(list, key, value))

/**
 *\brief 在单向链表开头插入节点
 *\param[in,out] list 单向链表头指针
 *\param[in] value 节点值
 */
#define slist_sp_prepend(list, key, value) ((list) = slist_sp_prepend(list, key, value))

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
