#include "heap.h"
#include "alloc/area.h"
#include "hhdm.h"
#include "klog.h"
#include "krlibc.h"
#include "lock.h"
#include "page.h"
#include "scheduler.h"

/* 内核堆扩容措施 */
void *heap_alloc(void *ptr, size_t size);

static struct mpool pool = {
    .cb_reqmem = heap_alloc,
};
static spin_t lock = SPIN_INIT;

uint64_t get_all_memusage() {
    return KERNEL_HEAP_SIZE;
}

static bool alloc_enter() {
    bool is_sti = are_interrupts_enabled();
    close_interrupt;
    spin_lock(lock);
    return is_sti;
}

static void alloc_exit(bool is_sti) {
    spin_unlock(lock);
    if (is_sti) open_interrupt;
}

void *malloc(size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_alloc(&pool, size);
    alloc_exit(is_sti);
    return ptr;
}

void free(void *ptr) {
    bool is_sti = alloc_enter();
    mpool_free(&pool, ptr);
    alloc_exit(is_sti);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    not_null_assets(ptr, "xmalloc failed");
    return ptr;
}

void *calloc(size_t n, size_t size) {
    if (__builtin_mul_overflow(n, size, &size)) return NULL;
    void *ptr = malloc(size);
    if (ptr == NULL) return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void *realloc(void *ptr, size_t newsize) {
    const bool is_sti = alloc_enter();
    void      *n_ptr  = mpool_realloc(&pool, ptr, newsize);
    alloc_exit(is_sti);
    return n_ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, align);
    alloc_exit(is_sti);
    return ptr;
}

size_t malloc_usable_size(void *ptr) {
    bool   is_sti = alloc_enter();
    size_t size   = mpool_msize(&pool, ptr);
    alloc_exit(is_sti);
    return size;
}

void *memalign(size_t align, size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, align);
    alloc_exit(is_sti);
    return ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, alignment);
    alloc_exit(is_sti);
    if (ptr == NULL) return 1;
    *memptr = ptr;
    return 0;
}

void *valloc(size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
    alloc_exit(is_sti);
    return ptr;
}

void *pvalloc(size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
    alloc_exit(is_sti);
    return ptr;
}

void *heap_alloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        uint64_t ptr0 = page_alloc_random(get_kernel_pagedir(), size, KERNEL_PTE_FLAGS);
        not_null_assets((void *)ptr0, "Out of memory in kernel heap");
        return (void *)ptr0;
    }
    if ((uint64_t)ptr > DRIVER_AREA_MEM)
        not_null_assets(NULL, "Out of memory in heap virtual area");
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)ptr, size, KERNEL_PTE_FLAGS);
    return ptr;
}

void init_heap() {
    uint64_t base_addr = KERNEL_HEAP_START;
    logkf("init heap at %p\n", base_addr);
    page_map_range_to_random(get_kernel_pagedir(), base_addr, KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, (void *)base_addr, KERNEL_HEAP_SIZE);
    switch_cp_kernel_page_directory();
}
