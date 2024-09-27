#include "../../../include/mpool.h"

void blk_setalloced(void *ptr, size_t size) {
    blk_head(ptr)       &= ~FREE_FLAG;
    blk_tail(ptr, size) &= ~FREE_FLAG;
}

void blk_setfreed(void *ptr, size_t size) {
    blk_head(ptr)       |= FREE_FLAG;
    blk_tail(ptr, size) |= FREE_FLAG;
}

void blk_set_area_4k(void *ptr, size_t size) {
    blk_head(ptr)       &= ~SIZE_FLAG;
    blk_tail(ptr, size) &= ~SIZE_FLAG;
}

void blk_setsize(void *ptr, size_t size) {
    ((size_t *)ptr)[-1]     = size;
    *(size_t *)(ptr + size) = size;
}

void blk_set_area_2M(void *ptr, size_t size) {
    blk_head(ptr)       |= SIZE_FLAG;
    blk_tail(ptr, size) |= SIZE_FLAG;
}

void *blk_mergeprev(void *ptr, blk_detach_t detach, void *data) {
    void  *prev = blk_prev(ptr);
    size_t size = blk_size(ptr) + blk_size(prev) + 2 * sizeof(size_t);
    if (detach) detach(data, prev);
    blk_setsize(prev, size);
    return prev;
}

void *blk_mergenext(void *ptr, blk_detach_t detach, void *data) {
    void  *next = blk_next(ptr);
    size_t size = blk_size(ptr) + blk_size(next) + 2 * sizeof(size_t);
    if (detach) detach(data, next);
    blk_setsize(ptr, size);
    return ptr;
}

void *blk_split(void *ptr, size_t size) {
    bool   is_2M   = blk_area_is_2M(ptr);
    size_t oldsize = blk_size(ptr);
    blk_setsize(ptr, size);
    if (is_2M) blk_set_area_2M(ptr, size);
    size_t offset = size + 2 * sizeof(size_t);
    blk_setsize(ptr + offset, oldsize - offset);
    if (is_2M) blk_set_area_2M(ptr + offset, oldsize - offset);
    return ptr + offset;
}

void *blk_trymerge(void *ptr, blk_detach_t detach, void *data) {
    bool   is_2M = blk_area_is_2M(ptr);
    size_t size  = blk_size(ptr);
    if (!blk_nonext(ptr, size) && (blk_nexthead(ptr, size) & FREE_FLAG)) { //
        ptr = blk_mergenext(ptr, detach, data);
    }
    if (!blk_noprev(ptr) && (blk_prevtail(ptr) & FREE_FLAG)) { //
        ptr = blk_mergeprev(ptr, detach, data);
    }
    if (is_2M) blk_set_area_2M(ptr, blk_size(ptr));
    return ptr;
}

void *blk_areaptr(void *ptr) {
    if (blk_area_is_2M(ptr)) {
        return (void *)((size_t)ptr & ~(SIZE_2M - 1));
    } else {
        return (void *)((size_t)ptr & ~(SIZE_4k - 1));
    }
}

void *blk_poolptr(void *ptr) {
    return (void *)(*(size_t *)blk_areaptr(ptr) & ~FLAG_BITS);
}

void *blk_prev(void *ptr) {
    if (blk_noprev(ptr)) return NULL;
    size_t prevsize = blk_prevtail(ptr) & ~FLAG_BITS;
    return ptr - 2 * sizeof(size_t) - prevsize;
}

void *blk_next(void *ptr) {
    size_t size = blk_size(ptr);
    if (blk_nonext(ptr, size)) return NULL;
    return ptr + size + 2 * sizeof(size_t);
}
