#include "heap.h"
#include "alloc/area.h"
#include "hhdm.h"
#include "krlibc.h"
#include "lock.h"
#include "page.h"
#include "scheduler.h"

static struct mpool pool;
static spin_t       lock = SPIN_INIT;

uint64_t get_all_memusage() {
    return KERNEL_HEAP_SIZE;
}

void *calloc(size_t n, size_t size) {
    if (__builtin_mul_overflow(n, size, &size)) return NULL;
    void *ptr = malloc(size);
    if (ptr == NULL) return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void *malloc(size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_alloc(&pool, size);
    spin_unlock(lock);
    open_interrupt;
    return ptr;
}

void free(void *ptr) {
    close_interrupt;
    spin_lock(lock);
    mpool_free(&pool, ptr);
    spin_unlock(lock);
    open_interrupt;
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    not_null_assets(ptr, "xmalloc failed");
    return ptr;
}

void *realloc(void *ptr, size_t newsize) {
    close_interrupt;
    spin_lock(lock);
    void *n_ptr = mpool_realloc(&pool, ptr, newsize);
    spin_unlock(lock);
    open_interrupt;
    return n_ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_aligned_alloc(&pool, size, align);
    spin_unlock(lock);
    open_interrupt;
    return ptr;
}

size_t malloc_usable_size(void *ptr) {
    close_interrupt;
    spin_lock(lock);
    size_t size = mpool_msize(&pool, ptr);
    spin_unlock(lock);
    open_interrupt;
    return size;
}

void *memalign(size_t align, size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_aligned_alloc(&pool, size, align);
    spin_unlock(lock);
    open_interrupt;
    return ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_aligned_alloc(&pool, size, alignment);
    spin_unlock(lock);
    open_interrupt;
    if (ptr == NULL) return 1;
    *memptr = ptr;
    return 0;
}

void *valloc(size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
    spin_unlock(lock);
    open_interrupt;
    return ptr;
}

void *pvalloc(size_t size) {
    close_interrupt;
    spin_lock(lock);
    void *ptr = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
    spin_unlock(lock);
    open_interrupt;
    return ptr;
}

void init_heap() {
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)phys_to_virt(KERNEL_HEAP_START),
                             KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, (void *)phys_to_virt(KERNEL_HEAP_START), KERNEL_HEAP_SIZE);
}
