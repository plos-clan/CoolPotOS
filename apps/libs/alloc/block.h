#pragma once

#include "../../include/alloc.h"

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

finline void blk_setalloced(void *ptr, size_t size) {
  blk_head(ptr)       &= ~FREE_FLAG;
  blk_tail(ptr, size) &= ~FREE_FLAG;
}
finline void blk_setfreed(void *ptr, size_t size) {
  blk_head(ptr)       |= FREE_FLAG;
  blk_tail(ptr, size) |= FREE_FLAG;
}

#define blk_size(ptr) (blk_head(ptr) & ~FLAG_BITS) // 获取块大小

/**
 *\brief 设置块的大小 (会清除块标记!)
 *
 *\param ptr      块指针
 *\param size     块大小
 */
finline void blk_setsize(void *ptr, size_t size) {
  ((size_t *)ptr)[-1]     = size;
  *(size_t *)(ptr + size) = size;
}

// 判断分配区大小是否为 2M，仅对多分配区有效
#define blk_area_is_2M(ptr) (blk_head(ptr) & SIZE_FLAG)
#define blk_area_is_4k(ptr) (!blk_area_is_2M(ptr))

finline void blk_set_area_4k(void *ptr, size_t size) {
  blk_head(ptr)       &= ~SIZE_FLAG;
  blk_tail(ptr, size) &= ~SIZE_FLAG;
}
finline void blk_set_area_2M(void *ptr, size_t size) {
  blk_head(ptr)       |= SIZE_FLAG;
  blk_tail(ptr, size) |= SIZE_FLAG;
}

// 获取分配区头指针，仅对多分配区有效
finline void *blk_areaptr(void *ptr) {
  if (blk_area_is_2M(ptr)) {
    return (void *)((size_t)ptr & ~(SIZE_2M - 1));
  } else {
    return (void *)((size_t)ptr & ~(SIZE_4k - 1));
  }
}
// 获取分配区结构指针，仅对多分配区有效
finline void *blk_poolptr(void *ptr) {
  return (void *)(*(size_t *)blk_areaptr(ptr) & ~FLAG_BITS);
}

// 获取上一个块的指针
finline void *blk_prev(void *ptr) {
  if (blk_noprev(ptr)) return null;
  size_t prevsize = blk_prevtail(ptr) & ~FLAG_BITS;
  return ptr - 2 * sizeof(size_t) - prevsize;
}
// 获取下一个块的指针
finline void *blk_next(void *ptr) {
  size_t size = blk_size(ptr);
  if (blk_nonext(ptr, size)) return null;
  return ptr + size + 2 * sizeof(size_t);
}

typedef void (*blk_detach_t)(void *data, void *ptr);

finline void *blk_mergeprev(void *ptr, blk_detach_t detach, void *data) {
  void  *prev = blk_prev(ptr);
  size_t size = blk_size(ptr) + blk_size(prev) + 2 * sizeof(size_t);
  if (detach) detach(data, prev);
  blk_setsize(prev, size);
  return prev;
}
finline void *blk_mergenext(void *ptr, blk_detach_t detach, void *data) {
  void  *next = blk_next(ptr);
  size_t size = blk_size(ptr) + blk_size(next) + 2 * sizeof(size_t);
  if (detach) detach(data, next);
  blk_setsize(ptr, size);
  return ptr;
}
/**
 *\brief 尝试合并前后相邻块 (会清除块标记!)
 *
 *\param ptr      块指针
 *\param detach   用于(可合并时)将相邻块从空闲链表中移除的回调
 *\param pool     内存池指针
 *\return 新的块指针
 */
finline void *blk_trymerge(void *ptr, blk_detach_t detach, void *data) {
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

/**
 *\brief 将一个块分割成两个 (会清除块标记!)
 *
 *\param ptr      块指针
 *\param size     分割出第一个块的大小
 *\return value
 */
finline void *blk_split(void *ptr, size_t size) {
  bool   is_2M   = blk_area_is_2M(ptr);
  size_t oldsize = blk_size(ptr);
  blk_setsize(ptr, size);
  if (is_2M) blk_set_area_2M(ptr, size);
  size_t offset = size + 2 * sizeof(size_t);
  blk_setsize(ptr + offset, oldsize - offset);
  if (is_2M) blk_set_area_2M(ptr + offset, oldsize - offset);
  return ptr + offset;
}
