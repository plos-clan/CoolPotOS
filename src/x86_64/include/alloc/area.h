#pragma once

#include "alloc.h"
#include "block.h"

// 获取分配区大小，仅对多分配区有效
#define allocarea_size(ptr) (((size_t *)ptr)[0] & SIZE_FLAG ? SIZE_2M : SIZE_4k)

static inline void allocarea_init(void *addr, size_t size, void *pdata) {
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

// 对多分配区无效 (会损坏分配区)
static inline void *allocarea_reinit(void *addr, size_t oldsize, size_t newsize) {
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
