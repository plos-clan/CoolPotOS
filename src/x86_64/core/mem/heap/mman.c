#include "alloc/alloc.h"
#include "alloc/area.h"
#include "alloc/block.h"
#include "alloc/freelist.h"
#include "alloc/large-blk.h"
#include "krlibc.h"

// mman_free 中使用的临时函数
// 用于将内存块从空闲链表中分离
static void _detach(mman_t man, freelist_t ptr) {
    int id = freelists_size2id(blk_size(ptr));
    if (id < 0) {
        man->large_blk = freelist_detach(man->large_blk, ptr);
    } else {
        freelists_detach(man->freed, id, ptr);
    }
}

bool mman_init(mman_t man, void *ptr, size_t size) {
    if (man == NULL || ptr == NULL || size == 0) return false;
    if (size != SIZE_2M && size != SIZE_4k) return false;
    man->main.ptr          = ptr;
    man->main.alloced_size = 0;
    man->main.next         = NULL;
    man->size              = size;
    man->alloced_size      = 0;
    man->cb_reqmem         = NULL;
    man->cb_delmem         = NULL;
    man->large_blk         = NULL;
#pragma unroll
    for (size_t i = 0; i < FREELIST_NUM; i++) {
        man->freed[i] = NULL;
    }
#pragma unroll
    for (size_t i = 0; i < LARGEBLKLIST_NUM; i++) {
        man->large[i] = NULL;
    }
    allocarea_init(ptr, size, &man->main);
    freelist_put(&man->large_blk, ptr + 2 * sizeof(size_t));
    return true;
}

void mman_deinit(mman_t man) {
    if (man == NULL) return;
    if (man->cb_delmem) {
        for (mman_pool_t p = man->main.next, n = p->next; p; p = n, n = p->next) {
            man->cb_delmem(p->ptr, allocarea_size(p->ptr));
        }
        man->cb_delmem(man->main.ptr, allocarea_size(man->main.ptr));
    }
    *man = (struct mman){};
}

size_t mman_alloced_size(mman_t man) {
    return man ? man->alloced_size : 0;
}

size_t mman_total_size(mman_t man) {
    return man ? man->size : 0;
}

void mman_setcb(mman_t man, cb_reqmem_t reqmem, cb_delmem_t delmem) {
    if (man == NULL) return;
    man->cb_reqmem = reqmem;
    man->cb_delmem = delmem;
}

#define RESERVED_SIZE (PADDING(sizeof(struct mman_pool)) + 6 * sizeof(size_t))

static bool mman_reqmem(mman_t man, size_t size) {
    size += sizeof(struct mman_pool) + 4 * sizeof(size_t);
    if (man->cb_reqmem == NULL) return false;
    if (size > SIZE_2M) return false;
#if !ALLOC_FORCE_2M_PAGE
    size = size > SIZE_4k ? SIZE_2M : SIZE_4k;
#else
    size = SIZE_2M;
#endif

    void *mem = man->cb_reqmem(NULL, size);
    if (mem == NULL) return false;

    mman_pool_t pool = mem + 2 * sizeof(size_t);
    allocarea_init(mem, size, pool);
    man->size += size;

    void *ptr = blk_split(pool, PADDING(sizeof(struct mman_pool)));
    blk_setalloced(pool, blk_size(pool));
    blk_setfreed(ptr, blk_size(ptr));

    pool->ptr          = mem;
    pool->alloced_size = 0;
    pool->next         = man->main.next;
    man->main.next     = pool;

    freelist_put(&man->large_blk, ptr);
    return true;
}

static bool mman_delmem(mman_t man, mman_pool_t pool) {
    if (man->cb_delmem == NULL) return false;

    mman_pool_t prev = &man->main;
    for (; prev != NULL; prev = prev->next) {
        if (prev->next == pool) break;
    }
    if (prev == NULL) return false; // error

    prev->next  = pool->next;
    size_t size = allocarea_size(pool->ptr);
    man->cb_delmem(pool->ptr, size);
    man->size -= size;

    return true;
}

// 将块标记为已释放并加入空闲链表
static void do_free(mman_t man, void *ptr) {
    blk_setfreed(ptr, blk_size(ptr));
    bool puted = freelists_put(man->freed, ptr);
    if (!puted) freelist_put(&man->large_blk, ptr);
}

static inline bool try_split_and_free(mman_t man, void *ptr, size_t size) {
    if (blk_size(ptr) >= size + 8 * sizeof(size_t)) {
        void *new_ptr = blk_split(ptr, size);
        do_free(man, new_ptr);
        return true;
    }
    return false;
}

void *mman_alloc(mman_t man, size_t size) {
    size = size == 0 ? 2 * sizeof(size_t) : PADDING(size); // 保证最小分配 2 个字长且对齐到 2 倍字长

    if (size >= ALLOC_LARGE_BLK_SIZE) {
        return large_blk_alloc(size, man->large, man->cb_reqmem, man->cb_delmem);
    }

    // 优先从空闲链表中分配
    void *ptr = freelists_match(man->freed, size) ?: freelist_match(&man->large_blk, size);

    if (ptr == NULL) { // 不足就分配
        if (!mman_reqmem(man, size)) return NULL;
        ptr = freelist_match(&man->large_blk, size);
    }

    try_split_and_free(man, ptr, size);

    size = blk_size(ptr);
    blk_setalloced(ptr, size);
    mman_pool_t pool    = blk_poolptr(ptr);
    pool->alloced_size += size;
    man->alloced_size  += size;
    return ptr;
}

void *mman_aligned_alloc(mman_t man, size_t size, size_t align) {
    if (align & (align - 1)) return NULL;                         // 不是 2 的幂次方
    if (align < 2 * sizeof(size_t)) return mman_alloc(man, size); // 对齐小于 2 倍指针大小
    size = size == 0 ? 2 * sizeof(size_t) : PADDING(size); // 保证最小分配 2 个字长且对齐到 2 倍字长

    if (size >= ALLOC_LARGE_BLK_SIZE || align >= ALLOC_LARGE_BLK_SIZE) {
        void *ptr = large_blk_alloc(size, man->large, man->cb_reqmem, man->cb_delmem);
        if ((size_t)ptr % align != 0) { // TODO 让这种情况永远不会出现
            large_blk_free(man->large, ptr, man->cb_delmem);
            ptr = NULL;
        }
        return ptr;
    }

    // 优先从空闲链表中分配
    void *ptr = freelists_aligned_match(man->freed, size, align)
                    ?: freelist_aligned_match(&man->large_blk, size, align);

    if (ptr == NULL) { // 不足就分配
        if (!mman_reqmem(man, size + align)) return NULL;
        ptr = freelist_aligned_match(&man->large_blk, size, align);
    }

    size_t offset = PADDING_UP(ptr, align) - (uint64_t)ptr;
    if (offset > 0) {
        void *new_ptr = blk_split(ptr, offset - 2 * sizeof(size_t));
        do_free(man, ptr);
        ptr = new_ptr;
    }

    try_split_and_free(man, ptr, size);

    size = blk_size(ptr);
    blk_setalloced(ptr, size);
    mman_pool_t pool    = blk_poolptr(ptr);
    pool->alloced_size += size;
    man->alloced_size  += size;
    return ptr;
}

void mman_free(mman_t man, void *ptr) {
    if (ptr == NULL) return;

    if ((size_t)ptr % SIZE_4k == 0) {
        if (large_blk_free(man->large, ptr, man->cb_delmem)) return;
    }

    size_t      size    = blk_size(ptr);
    mman_pool_t pool    = blk_poolptr(ptr);
    pool->alloced_size -= size;
    man->alloced_size  -= size;

    ptr = blk_trymerge(ptr, (blk_detach_t)_detach, man);
    if (pool != &man->main && pool->alloced_size == 0) {
        if (mman_delmem(man, pool)) return;
    }
    do_free(man, ptr);
}

size_t mman_msize(mman_t man, void *ptr) {
    if (ptr == NULL) return 0;
    return blk_size(ptr);
}

void *mman_realloc(mman_t man, void *ptr, size_t newsize) {
    if (ptr == NULL) return mman_alloc(man, newsize);
    if (newsize == 0) newsize = 2 * sizeof(size_t);
    newsize = PADDING(newsize);

    size_t size = blk_size(ptr);
    if (size >= newsize) {
        if ((size_t)ptr % SIZE_4k == 0) {
            if (size - newsize >= 2 * SIZE_4k) goto L1;
        } else {
            try_split_and_free(man, ptr, newsize);
        }
        return ptr;
    }
L1:

    if ((size_t)ptr % SIZE_4k != 0) {
        void *next = blk_next(ptr);
        if (next != NULL && blk_freed(next)) {
            size_t total_size = size + 2 * sizeof(size_t) + blk_size(next);
            if (total_size >= newsize) {
                ptr = blk_mergenext(ptr, (blk_detach_t)_detach, man);
                try_split_and_free(man, ptr, newsize);
                return ptr;
            }
        }
    }

    void *new_ptr = mman_alloc(man, newsize);
    memcpy(new_ptr, ptr, size);
    mman_free(man, ptr);
    return new_ptr;
}

//! 注意 align 和第一次分配时的 align 必须相同
void *mman_aligned_realloc(mman_t man, void *ptr, size_t newsize, size_t align) {
    if (ptr == NULL) return mman_aligned_alloc(man, newsize, align);
    if (align & (align - 1)) return NULL;                                   // 不是 2 的幂次方
    if (align < 2 * sizeof(size_t)) return mman_realloc(man, ptr, newsize); // 对齐小于 2 倍指针大小
    newsize = newsize == 0 ? 2 * sizeof(size_t) : PADDING(newsize);

    size_t size = blk_size(ptr);
    if (size >= newsize) {
        if ((size_t)ptr % SIZE_4k == 0) {
            if (size - newsize >= 2 * SIZE_4k) goto L1;
        } else {
            try_split_and_free(man, ptr, newsize);
        }
        return ptr;
    }
L1:

    if ((size_t)ptr % SIZE_4k != 0) {
        void *next = blk_next(ptr);
        if (next != NULL && blk_freed(next)) {
            size_t total_size = size + 2 * sizeof(size_t) + blk_size(next);
            if (total_size >= newsize) {
                ptr = blk_mergenext(ptr, (blk_detach_t)_detach, man);
                try_split_and_free(man, ptr, newsize);
                return ptr;
            }
        }
    }

    void *new_ptr = mman_aligned_alloc(man, newsize, align);
    memcpy(new_ptr, ptr, size);
    mman_free(man, ptr);
    return new_ptr;
}
