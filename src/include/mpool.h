#ifndef CRASHPOWEROS_MPOOL_H
#define CRASHPOWEROS_MPOOL_H

#define FREELIST_NUM 8
#define FREELIST_MAXBLKSIZE 16384

#define SIZE_4k ((size_t)4096)
#define SIZE_2M ((size_t)(2 * 1024 * 1024))

#define MALLOC_PADDING(size) (((size) + 2 * sizeof(size_t) - 1) & ~(2 * sizeof(size_t) - 1))

#define FREE_FLAG ((size_t)1) // 块释放标记
#define AREA_FLAG ((size_t)2) // 分配区头尾标记 (防止块查找越界)
#define SIZE_FLAG ((size_t)4) // 2M 分配区标记
#define FLAG_BITS ((size_t)7) // 所有标志位

// 两倍字长对齐和 16k 对齐
#define PADDING(size)     (((size) + 2 * sizeof(size_t) - 1) & ~(2 * sizeof(size_t) - 1))
#define PADDING_4k(size)  (((size) + SIZE_4k - 1) & ~(size_t)(SIZE_4k - 1))
#define PADDING_16k(size) (((size) + 16384 - 1) & ~(size_t)(16384 - 1))
#define PADDING_2M(size)  (((size) + SIZE_2M - 1) & ~(size_t)(SIZE_2M - 1))

#define blk_prevtail(ptr)       (((size_t *)ptr)[-2])
#define blk_head(ptr)           (((size_t *)ptr)[-1])
#define blk_tail(ptr, size)     (((size_t *)(ptr + size))[0])
#define blk_nexthead(ptr, size) (((size_t *)(ptr + size))[1])

#define blk_noprev(ptr)       ((bool)(blk_prevtail(ptr) & AREA_FLAG))       // 是否有上一个块
#define blk_nonext(ptr, size) ((bool)(blk_nexthead(ptr, size) & AREA_FLAG)) // 是否有下一个块

#define blk_freed(ptr)   (blk_head(ptr) & FREE_FLAG) // 是否已释放
#define blk_alloced(ptr) (!blk_freed(ptr))           // 是否已分配

#define blk_size(ptr) (blk_head(ptr) & ~FLAG_BITS) // 获取块大小
#define blk_area_is_2M(ptr) (blk_head(ptr) & SIZE_FLAG)
#define blk_area_is_4k(ptr) (!blk_area_is_2M(ptr))

#include "common.h"

typedef struct freelist *freelist_t;
typedef freelist_t freelists_t[FREELIST_NUM];
typedef void *(*cb_reqmem_t)(void *ptr, size_t size);
typedef void (*cb_delmem_t)(void *ptr, size_t size);
typedef void (*blk_detach_t)(void *data, void *ptr);

struct freelist {
    freelist_t next;
    freelist_t prev;
};

typedef struct mpool {
    void       *ptr;          // 指向内存区的指针
    size_t      size;         // 内存区总大小
    size_t      alloced_size; // 已分配的内存大小
    cb_reqmem_t cb_reqmem;    // 请求内存的回调函数
    cb_delmem_t cb_delmem;    // 释放内存的回调函数
    freelist_t  large_blk;    // 大块内存的空闲链表
    freelists_t freed;        // 小块内存的空闲链表 (组)
} *mpool_t;

void blk_setalloced(void *ptr, size_t size);
void blk_setfreed(void *ptr, size_t size);
void blk_set_area_4k(void *ptr, size_t size);
void blk_setsize(void *ptr, size_t size);
void blk_set_area_2M(void *ptr, size_t size);
void *blk_split(void *ptr, size_t size);
void *blk_trymerge(void *ptr, blk_detach_t detach, void *data);
void *blk_mergeprev(void *ptr, blk_detach_t detach, void *data);
void *blk_mergenext(void *ptr, blk_detach_t detach, void *data);
void *blk_areaptr(void *ptr);
void *blk_poolptr(void *ptr);
void *blk_prev(void *ptr);
void *blk_next(void *ptr);

void freelist_put(freelist_t *list_p, freelist_t ptr);
void freelist_put(freelist_t *list_p, freelist_t ptr);
bool freelists_put(freelists_t lists, void *_ptr);
int freelists_size2id(size_t size);
freelist_t freelist_detach(freelist_t list, freelist_t ptr);
void *freelists_detach(freelists_t lists, int id, freelist_t ptr);
void *freelist_match(freelist_t *list_p, size_t size);
void *freelists_match(freelists_t lists, size_t size);

bool mpool_init(mpool_t pool, void *ptr, size_t size);
void mpool_setcb(mpool_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem);
void _detach(mpool_t pool, freelist_t ptr);
void mpool_deinit(mpool_t pool);
size_t mpool_total_size(mpool_t pool);
size_t mpool_alloced_size(mpool_t pool);
void *allocarea_reinit(void *addr, size_t oldsize, size_t newsize);
void *mpool_alloc(mpool_t pool, size_t size);
void mpool_free(mpool_t pool, void *ptr);
size_t mpool_msize(mpool_t pool, void *ptr);
void *mpool_realloc(mpool_t pool, void *ptr, size_t newsize);

#endif
