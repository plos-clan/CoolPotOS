#include "alloc/alloc.h"

void sized_mpool_init(sized_mpool_t pool, void *ptr, size_t bsize, size_t len) {
    pool->ptr      = ptr;
    pool->size     = bsize * len;
    pool->bsize    = bsize;
    pool->len      = len;
    pool->nalloced = 0;
    pool->freelist = ptr;
    for (size_t i = 1; i < len; i++) {
        *(void **)(ptr + (i - 1) * bsize) = ptr + i * bsize;
    }
    *(void **)(ptr + (len - 1) * bsize) = NULL;
}

void *sized_mpool_alloc(sized_mpool_t pool) {
    if (pool == NULL || pool->freelist == NULL) return NULL;
    void **ptr     = pool->freelist;
    pool->freelist = *ptr;
    return ptr;
}

void sized_mpool_free(sized_mpool_t pool, void *ptr) {
    if (pool == NULL || ptr == NULL) return;
    *(void **)ptr  = pool->freelist;
    pool->freelist = ptr;
}

bool sized_mpool_inpool(sized_mpool_t pool, void *ptr) {
    if (pool == NULL || ptr == NULL) return false;
    if (ptr < pool->ptr || pool->ptr + pool->size <= ptr) return false;
    return true;
}
