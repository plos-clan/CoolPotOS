#pragma once

#include "../../include/alloc.h"

// ALLOC_LARGE_BLK_SIZE

#include "freelist.h"

struct large_blk {
  large_blk_t next;
  large_blk_t prev;
  void       *ptr;
  size_t      size;
};

finline large_blk_t *large_blk_blokp(large_blks_t blks, void *ptr) __attr(pure);

finline large_blk_t *large_blk_blokp(large_blks_t blks, void *ptr) {
    large_blk_t *ret = blks + ((size_t)ptr / SIZE_4k) % LARGEBLKLIST_NUM;
  return ret;
}

finline large_blk_t large_blk_put(large_blks_t blks, void *ptr, size_t size) {
  large_blk_t *blk_p = large_blk_blokp(blks, ptr);
  large_blk_t  blk   = malloc(sizeof(struct large_blk));
  if (blk == null) return null;
  blk->ptr  = ptr;
  blk->size = size;
  freelist_put((freelist_t *)blk_p, (freelist_t)blk);
  return blk;
}

finline large_blk_t large_blk_find(large_blks_t blks, void *ptr) {
  large_blk_t *blk_p = large_blk_blokp(blks, ptr);
  for (large_blk_t blk = *blk_p; blk; blk = blk->next) {
    if (blk->ptr == ptr) return blk;
  }
  return null;
}

// 保证 size 大于 ALLOC_LARGE_BLK_SIZE
finline void *large_blk_alloc(size_t size, large_blks_t blks, cb_reqmem_t reqmem,
                              cb_delmem_t delmem) {
  size      = PADDING_4k(size);
  void *ptr = reqmem(null, size);
  if (ptr == null) return null;
  large_blk_t blk = large_blk_put(blks, ptr, size);
  if (blk == null) {
    delmem(ptr, size);
    return null;
  }
  blk->ptr  = ptr;
  blk->size = size;
  blk->prev = null;
  blk->next = null;
  return ptr;
}

finline bool large_blk_free(large_blks_t blks, void *ptr, cb_delmem_t delmem) {
  large_blk_t *blk_p = large_blk_blokp(blks, ptr);
  auto         blk   = large_blk_find(blks, ptr);
  if (blk == null) return false;

  *blk_p = (large_blk_t)freelist_detach((freelist_t)*blk_p, (freelist_t)blk);

  delmem(blk->ptr, blk->size);
  free(blk);
  return true;
}
