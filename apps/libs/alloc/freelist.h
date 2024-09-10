#pragma once

#include "../../include/alloc.h"

#include "block.h"

// 大于 16k 的都算作大内存块
// 大内存块使用专门的 freelist
#define FREELIST_MAXBLKSIZE 16384

struct freelist {
  freelist_t next;
  freelist_t prev;
};

finline int freelists_size2id(size_t size) __attr(const);

//; 获取该大小属于freelists中的哪个list
finline int freelists_size2id(size_t size) {
  if (size < 64) return 0;
  if (size < 256) return 1;
  if (size >= FREELIST_MAXBLKSIZE) return -1;
  return (32 - 9) - __builtin_clz(size) + 2;
}

finline freelist_t freelist_detach(freelist_t list, freelist_t ptr) {
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
 *\return value
 */
finline void *freelists_detach(freelists_t lists, int id, freelist_t ptr) {
  if (lists[id] == ptr) {
    lists[id] = ptr->next;
    return ptr;
  }

  if (ptr->next) ptr->next->prev = ptr->prev;
  if (ptr->prev) ptr->prev->next = ptr->next;
  return ptr;
}

finline void *freelist_match(freelist_t *list_p, size_t size) {
  for (freelist_t list = *list_p; list != null; list = list->next) {
    size_t tgt_size = blk_size(list);
    if (tgt_size >= size) {
      *list_p = freelist_detach(*list_p, list);
      return list;
    }
  }
  return null;
}

/**
 *\brief 匹配并将内存从 freelist 中分离
 *
 *\param lists    空闲链表 (组)
 *\param size     要寻找内存的最小大小
 *\return 找到的内存块指针，未找到为 null
 */
finline void *freelists_match(freelists_t lists, size_t size) {
  int id = freelists_size2id(size);
  if (id < 0) return null;
  for (; id < FREELIST_NUM; id++) {
    for (freelist_t list = lists[id]; list != null; list = list->next) {
      size_t tgt_size = blk_size(list);
      if (tgt_size >= size) return freelists_detach(lists, id, list);
    }
  }
  return null;
}

/**
 *\brief 将元素放到 freelist 中
 *
 *\param list_p   空闲链表指针
 *\param ptr      内存块指针
 */
finline void freelist_put(freelist_t *list_p, freelist_t ptr) {
  ptr->next = *list_p;
  ptr->prev = null;
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
finline bool freelists_put(freelists_t lists, void *_ptr) {
  freelist_t ptr  = _ptr;
  size_t     size = blk_size(ptr);
  int        id   = freelists_size2id(size);
  if (id < 0) return false;
  ptr->next = lists[id];
  ptr->prev = null;
  lists[id] = ptr;
  if (ptr->next) ptr->next->prev = ptr;
  return true;
}
