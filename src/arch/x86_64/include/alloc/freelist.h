#pragma once

#include "block.h"
#include "krlibc.h"

// 大于 16k 的都算作大内存块
// 大内存块使用专门的 freelist
#define FREELIST_MAXBLKSIZE 16384

struct freelist {
    freelist_t next;
    freelist_t prev;
};

static inline int freelists_size2id(size_t size) __attr(const);

//; 获取该大小属于freelists中的哪个list
static inline int freelists_size2id(size_t size) {
    if (size < 64) return 0;
    if (size < 256) return 1;
    if (size >= FREELIST_MAXBLKSIZE) return -1;
    return (32 - 9) - __builtin_clz(size) + 2;
}

/**
 *\brief 将 freelist 中的内存块分离
 *
 *\param list     空闲链表
 *\param ptr      要分离的内存块指针
 *\return 分离后的空闲链表
 */
static inline freelist_t freelist_detach(freelist_t list, freelist_t ptr) {
    if (list == ptr) return ptr->next;
    if (ptr->next) ptr->next->prev = ptr->prev;
    if (ptr->prev) ptr->prev->next = ptr->next;
    return list;
}

/**
 *\brief 将 freelist 中的内存块分离
 *
 *\param lists    空闲链表组
 *\param id       空闲链表的 id
 *\param ptr      要分离的内存块指针
 *\return 分离的内存块指针 (即传入的 ptr)
 */
static inline void *freelists_detach(freelists_t lists, int id, freelist_t ptr) {
    if (lists[id] == ptr) {
        lists[id] = ptr->next;
        return ptr;
    }

    if (ptr->next) ptr->next->prev = ptr->prev;
    if (ptr->prev) ptr->prev->next = ptr->next;
    return ptr;
}

/**
 *\brief 匹配并将内存从 freelist 中分离
 *
 *\param list_p   空闲链表指针
 *\param size     要寻找内存的最小大小
 *\return 找到的内存块指针，未找到为 NULL
 */
static inline void *freelist_match(freelist_t *list_p, size_t size) {
    for (freelist_t list = *list_p; list != NULL; list = list->next) {
        size_t tgt_size = blk_size(list);
        if (tgt_size >= size) {
            *list_p = freelist_detach(*list_p, list);
            return list;
        }
    }
    return NULL;
}

/**
 *\brief 匹配并将内存从 freelist 中分离
 *
 *\param lists    空闲链表 (组)
 *\param size     要寻找内存的最小大小
 *\return 找到的内存块指针，未找到为 NULL
 */
static inline void *freelists_match(freelists_t lists, size_t size) {
    int id = freelists_size2id(size);
    if (id < 0) return NULL;
    for (; id < FREELIST_NUM; id++) {
        for (freelist_t list = lists[id]; list != NULL; list = list->next) {
            size_t tgt_size = blk_size(list);
            if (tgt_size >= size) return freelists_detach(lists, id, list);
        }
    }
    return NULL;
}

#define aligned_size_of(ptr, align)                                                                \
    ((ssize_t)blk_size(ptr) - (ssize_t)(PADDING_UP(ptr, align) - (size_t)(ptr)))

/**
 *\brief 匹配并将内存从 freelist 中分离
 *       要求内存能对齐到 align 指定的大小且对齐后至少有 size 的大小
 *
 *\param list_p   空闲链表指针
 *\param size     要寻找内存的最小大小
 *\param align    对齐大小 (必须为 2 的幂) (必须大于等于 2 倍字长)
 *\return 找到的内存块指针，未找到为 NULL
 */
static inline void *freelist_aligned_match(freelist_t *list_p, size_t size, size_t align) {
    for (freelist_t list = *list_p; list != NULL; list = list->next) {
        ssize_t tgt_size = aligned_size_of(list, align);
        if (tgt_size >= (ssize_t)size) {
            *list_p = freelist_detach(*list_p, list);
            return list;
        }
    }
    return NULL;
}

/**
 *\brief 匹配并将内存从 freelist 中分离
 *       要求内存能对齐到 align 指定的大小且对齐后至少有 size 的大小
 *
 *\param lists    空闲链表 (组)
 *\param size     要寻找内存的最小大小
 *\param align    对齐大小 (必须为 2 的幂) (必须大于等于 2 倍字长)
 *\return 找到的内存块指针，未找到为 NULL
 */
static inline void *freelists_aligned_match(freelists_t lists, size_t size, size_t align) {
    int id = freelists_size2id(size);
    if (id < 0) return NULL;
    for (; id < FREELIST_NUM; id++) {
        for (freelist_t list = lists[id]; list != NULL; list = list->next) {
            ssize_t tgt_size = aligned_size_of(list, align);
            if (tgt_size >= (ssize_t)size) return freelists_detach(lists, id, list);
        }
    }
    return NULL;
}

#undef aligned_size_of

/**
 *\brief 将元素放到 freelist 中
 *
 *\param list_p   空闲链表指针
 *\param ptr      内存块指针
 */
static inline void freelist_put(freelist_t *list_p, freelist_t ptr) {
    ptr->next = *list_p;
    ptr->prev = NULL;
    *list_p   = ptr;
    if (ptr->next) ptr->next->prev = ptr;
}

/**
 *\brief 将元素放到 freelist 中
 *
 * 函数若失败应将内存块放到大内存块空闲链表中
 *
 *\param lists    空闲链表 (组)
 *\param ptr      内存块指针
 *\return 是否成功
 */
static inline bool freelists_put(freelists_t lists, void *_ptr) {
    freelist_t ptr  = _ptr;
    size_t     size = blk_size(ptr);
    int        id   = freelists_size2id(size);
    if (id < 0) return false;
    ptr->next = lists[id];
    ptr->prev = NULL;
    lists[id] = ptr;
    if (ptr->next) ptr->next->prev = ptr;
    return true;
}
