/*
 * Copi143 new alloc library
 */
#include "../../include/mpool.h"
#include "../../include/memory.h"
#include "../../include/printf.h"

static void allocarea_init(void *addr, size_t size, void *pdata) {
    size_t *ptr       = addr;
    size_t  size_flag = (size == SIZE_2M ? SIZE_FLAG : 0);

    // 设置分配区标记
    size_t len  = size / sizeof(size_t);
    size_t data = (size_t)pdata | AREA_FLAG | size_flag;
    ptr[0] = ptr[len - 1] = data; // 将首尾设为占位符

    // 设置第一个块
    ptr += 2, size -= 4 * sizeof(size_t);
    blk_setsize(ptr, size);
    blk_setfreed(ptr, size);
    if (size_flag) {
        blk_set_area_2M(ptr, size);
    } else {
        blk_set_area_4k(ptr, size);
    }
}

static bool mpool_reqmem(mpool_t pool, size_t size) {
    if (pool->cb_reqmem == NULL) return false;
    size_t memsize = PADDING_16k(size);

    void *mem = pool->cb_reqmem(pool->ptr + pool->size, memsize);
    if (mem == NULL) return false;
    if (mem != pool->ptr) {
        if (pool->cb_delmem) pool->cb_delmem(mem, memsize);
        return false;
    }

    void *ptr   = allocarea_reinit(pool->ptr, pool->size, pool->size + memsize);
    pool->size += memsize;

    ptr = blk_trymerge(ptr, (blk_detach_t)_detach, pool);
    blk_setfreed(ptr, blk_size(ptr));
    freelist_put(&pool->large_blk, ptr);
    return true;
}

// 将块标记为已释放并加入空闲链表
static void do_free(mpool_t pool, void *ptr) {
    blk_setfreed(ptr, blk_size(ptr));
    bool puted = freelists_put(pool->freed, ptr);
    if (!puted) freelist_put(&pool->large_blk, ptr);
}

static bool try_split_and_free(mpool_t pool, void *ptr, size_t size) {
    if (blk_size(ptr) >= size + 8 * sizeof(size_t)) {
        void *new_ptr = blk_split(ptr, size);
        do_free(pool, new_ptr);
        return true;
    }
    return false;
}

void mpool_setcb(mpool_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem) {
    if (pool == NULL) return;
    pool->cb_reqmem = reqmem;
    pool->cb_delmem = delmem;
}

void *allocarea_reinit(void *addr, size_t oldsize, size_t newsize) {
    if (newsize == oldsize) return NULL;

    size_t *ptr  = addr;
    size_t  len  = newsize / sizeof(size_t);
    size_t  data = (ptr[0] & ~FLAG_BITS) | AREA_FLAG | (newsize == SIZE_2M ? SIZE_FLAG : 0);
    ptr[0] = ptr[len - 1] = data; // 将首尾设为占位符

    if (newsize < oldsize) return NULL;

    void *blk = addr + oldsize + sizeof(size_t);
    blk_setsize(blk, newsize - oldsize - 2 * sizeof(size_t));
    return blk;
}

void *mpool_alloc(mpool_t pool, size_t size) {
    if (size == 0) size = 2 * sizeof(size_t);
    size = PADDING(size);

    void *ptr = freelists_match(pool->freed, size);

    if (ptr == NULL) ptr = freelist_match(&pool->large_blk, size);

    if (ptr == NULL) { // 不足就分配
        if (!mpool_reqmem(pool, size)) return NULL;
        ptr = freelist_match(&pool->large_blk, size);
    }

    try_split_and_free(pool, ptr, size);

    size = blk_size(ptr);
    blk_setalloced(ptr, size);
    pool->alloced_size += size;
    return ptr;
}

void mpool_free(mpool_t pool, void *ptr) {
    if (ptr == NULL) return;

    pool->alloced_size -= blk_size(ptr);

    ptr = blk_trymerge(ptr, (blk_detach_t)_detach, pool);
    do_free(pool, ptr);
}

size_t mpool_msize(mpool_t pool, void *ptr) {
    if (ptr == NULL) return 0;
    return blk_size(ptr);
}

void *mpool_realloc(mpool_t pool, void *ptr, size_t newsize) {
    if (ptr == NULL) return mpool_alloc(pool, newsize);
    if (newsize == 0) newsize = 2 * sizeof(size_t);
    newsize = PADDING(newsize);

    size_t size = blk_size(ptr);
    if (size >= newsize) {
        try_split_and_free(pool, ptr, newsize);
        return ptr;
    }

    void *next = blk_next(ptr);
    if (next != NULL && blk_freed(next)) {
        size_t total_size = size + 2 * sizeof(size_t) + blk_size(next);
        if (total_size >= newsize) {
            ptr = blk_mergenext(ptr, (blk_detach_t)_detach, pool);
            try_split_and_free(pool, ptr, newsize);
            return ptr;
        }
    }

    void *new_ptr = mpool_alloc(pool, newsize);
    memcpy(new_ptr, ptr, size);
    mpool_free(pool, ptr);
    return new_ptr;
}

void _detach(mpool_t pool, freelist_t ptr) {
    int id = freelists_size2id(blk_size(ptr));
    if (id < 0) {
        pool->large_blk = freelist_detach(pool->large_blk, ptr);
    } else {
        freelists_detach(pool->freed, id, ptr);
    }
}

void mpool_deinit(mpool_t pool) {
    if (pool == NULL) return;
    *pool = (struct mpool){};
}

size_t mpool_alloced_size(mpool_t pool) {
    return pool ? pool->alloced_size : 0;
}

size_t mpool_total_size(mpool_t pool) {
    return pool ? pool->size : 0;
}

bool mpool_init(mpool_t pool, void *ptr, size_t size) {
    if (pool == NULL || ptr == NULL || size == 0) return false;
    if (size & (2 * sizeof(size_t) - 1)) return false;
    pool->ptr          = ptr;
    pool->size         = size;
    pool->alloced_size = 0;
    pool->large_blk    = NULL;
    pool->cb_delmem    = NULL;
    pool->cb_reqmem    = NULL;
#pragma unroll
    for (size_t i = 0; i < FREELIST_NUM; i++) {
        pool->freed[i] = NULL;
    }
    allocarea_init(ptr, size, NULL);
    freelist_put(&pool->large_blk, ptr + 2 * sizeof(size_t));
    return true;
}
