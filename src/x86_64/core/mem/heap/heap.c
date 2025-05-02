#include "heap.h"
#include "hhdm.h"
#include "krlibc.h"
#include "page.h"
#include "alloc/area.h"
#include "lock.h"
#include "scheduler.h"

#undef malloc
#undef free
#undef get_allocator_size
#undef realloc

static struct mman mman;
static volatile spin_t mman_lock = SPIN_INIT;

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
    spin_lock(mman_lock);
    void *ptr = mman_alloc(&mman, size);
    spin_unlock(mman_lock);
    open_interrupt;
    return ptr;
}

void free(void *ptr) {
    close_interrupt;
    spin_lock(mman_lock);
    mman_free(&mman, ptr);
    spin_unlock(mman_lock);
    open_interrupt;
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    not_null_assets(ptr,"xmalloc failed");
    return ptr;
}

void *realloc(void *ptr, size_t newsize) {
    close_interrupt;
    spin_lock(mman_lock);
    void *n_ptr = mman_realloc(&mman, ptr, newsize);
    spin_unlock(mman_lock);
    open_interrupt;
    return n_ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    close_interrupt;
    spin_lock(mman_lock);
    void *ptr = mman_aligned_alloc(&mman, size, align);
    spin_unlock(mman_lock);
    open_interrupt;
    return ptr;
}

size_t malloc_usable_size(void *ptr) {
    close_interrupt;
    spin_lock(mman_lock);
    size_t size = mman_msize(&mman, ptr);
    spin_unlock(mman_lock);
    open_interrupt;
    return size;
}

void *memalign(size_t align, size_t size) {
    close_interrupt;
    spin_lock(mman_lock);
    void *ptr = mman_aligned_alloc(&mman, size, align);
    spin_unlock(mman_lock);
    open_interrupt;
    return ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    close_interrupt;
    spin_lock(mman_lock);
    void *ptr = mman_aligned_alloc(&mman, size, alignment);
    spin_unlock(mman_lock);
    open_interrupt;
    if (ptr == NULL) return 1;
    *memptr = ptr;
    return 0;
}

void *valloc(size_t size) {
    close_interrupt;
    spin_lock(mman_lock);
    void *ptr = mman_aligned_alloc(&mman, size, PAGE_SIZE);
    spin_unlock(mman_lock);
    open_interrupt;
    return ptr;
}

void *pvalloc(size_t size) {
    close_interrupt;
    spin_lock(mman_lock);
    void *ptr = mman_aligned_alloc(&mman, size, PAGE_SIZE);
    spin_unlock(mman_lock);
    open_interrupt;
    return ptr;
}

void init_heap() {
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)phys_to_virt(KERNEL_HEAP_START),
                             KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mman_init(&mman, (void *)phys_to_virt(KERNEL_HEAP_START), KERNEL_HEAP_SIZE);
}
