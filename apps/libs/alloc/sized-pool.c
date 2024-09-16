// This code is released under the MIT License

#include "../../include/syscall.h"

dlexport void sized_mpool_init(sized_mpool_t pool, void *ptr, size_t bsize, size_t len) {
  pool->ptr      = ptr;
  pool->size     = bsize * len;
  pool->bsize    = bsize;
  pool->len      = len;
  pool->nalloced = 0;
  pool->freelist = ptr;
  for (size_t i = 1; i < len; i++) {
    *(void **)(ptr + (i - 1) * bsize) = ptr + i * bsize;
  }
  *(void **)(ptr + (len - 1) * bsize) = null;
}

dlexport void *sized_mpool_alloc(sized_mpool_t pool) {
  if (pool == null || pool->freelist == null) return null;
  void **ptr     = pool->freelist;
  pool->freelist = *ptr;
  return ptr;
}

dlexport void sized_mpool_free(sized_mpool_t pool, void *ptr) {
  if (pool == null || ptr == null) return;
  *(void **)ptr  = pool->freelist;
  pool->freelist = ptr;
}

dlexport bool sized_mpool_inpool(sized_mpool_t pool, void *ptr) {
  if (pool == null || ptr == null) return false;
  if (ptr < pool->ptr || pool->ptr + pool->size <= ptr) return false;
  return true;
}
