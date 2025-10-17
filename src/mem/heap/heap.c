#include "mem/heap.h"
#include "mem/page.h"
#include "krlibc.h"
#include "lock.h"
#include "term/klog.h"

// #define HEAP_CHECK 1 //启用内核堆双端越界检查

static void *heap_alloc(void *ptr, size_t size);

static struct mpool pool = {
    .cb_reqmem = heap_alloc,
};

static spin_t lock = SPIN_INIT;

static bool alloc_enter() {
    bool is_sti = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(lock);
    return is_sti;
}

static void alloc_exit(bool is_sti) {
    spin_unlock(lock);
    if (is_sti) arch_open_interrupt();
}

static void *heap_alloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        uint64_t ptr0 = page_alloc_random(get_kernel_pagedir(), size, KERNEL_PTE_FLAGS);
        not_null_assert((void *)ptr0, "kernel_heap: Out of memory in kernel heap");
        return (void *)ptr0;
    }
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)ptr, size, KERNEL_PTE_FLAGS);
    return ptr;
}

// 检查措施实现
const static size_t start_magic = 0xF3EACFC1CCEBFAD7;
const static size_t end_magic   = 0xA2BAD9BE14335FE2;

size_t alloc_mem_size = 0;

static size_t get_true_size(size_t data_size) {
    return sizeof(size_t) + sizeof(start_magic) + data_size + sizeof(end_magic);
}

static void *set_magic(void *ptr, size_t data_size, bool fill_mem) {
    void *size_ptr, *start_magic_ptr, *data_ptr, *end_magic_ptr;
    size_ptr         = ptr;
    start_magic_ptr  = size_ptr + sizeof(size_t);
    data_ptr         = start_magic_ptr + sizeof(start_magic);
    end_magic_ptr    = data_ptr + data_size;
    alloc_mem_size  += data_size;

    if (fill_mem) memset(ptr, 0xFF, get_true_size(data_size));
    *(size_t *)size_ptr        = data_size;
    *(size_t *)start_magic_ptr = start_magic;
    *(size_t *)end_magic_ptr   = end_magic;
    return data_ptr;
}

void *check_magic(void *ptr, bool fill_mem) {
    void *size_ptr, *start_magic_ptr, *data_ptr, *end_magic_ptr;
    data_ptr          = ptr;
    start_magic_ptr   = data_ptr - sizeof(start_magic);
    size_ptr          = start_magic_ptr - sizeof(size_t);
    size_t data_size  = *(size_t *)size_ptr;
    end_magic_ptr     = data_ptr + data_size;
    alloc_mem_size   -= data_size;
    if (*(size_t *)start_magic_ptr != start_magic) {
        logkf("\nMemory checkout error START\n");
        arch_close_interrupt();
        arch_wait_for_interrupt();
    }
    if (*(size_t *)end_magic_ptr != end_magic) {
        logkf("\nMemory checkout error END\n");
        arch_close_interrupt();
        arch_wait_for_interrupt();
    }
    if (fill_mem) memset(size_ptr, 0xff, get_true_size(data_size));
    return size_ptr;
}
//

void *malloc(size_t size) {
    const bool is_sti = alloc_enter();

#ifdef HEAP_CHECK
    size             = (size + 7) & ~7;
    size_t true_size = get_true_size(size);
    void  *ptr       = mpool_alloc(&pool, true_size);
    if (!ptr) {
        logkf("\nkernel malloc null\n");
        close_interrupt;
        cpu_hlt;
    }
    ptr = set_magic(ptr, size, true);
#else
    void *ptr = mpool_alloc(&pool, size);
#endif

    alloc_exit(is_sti);
    return ptr;
}

void free(void *ptr) {
    if (!ptr) return;
    bool is_sti = alloc_enter();
#ifdef HEAP_CHECK
    ptr = check_magic(ptr, true);
#endif
    mpool_free(&pool, ptr);
    alloc_exit(is_sti);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    not_null_assert(ptr, "xmalloc failed");
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
#ifdef HEAP_CHECK
    if (ptr != NULL) ptr = check_magic(ptr, false);
    newsize          = (newsize + 7) & ~7;
    size_t true_size = get_true_size(newsize);

    ptr = mpool_realloc(&pool, ptr, true_size);
    ptr = set_magic(ptr, newsize, false);
#else
    ptr = mpool_realloc(&pool, ptr, newsize);
#endif
    alloc_exit(is_sti);
    return ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    const bool is_sti = alloc_enter();
#ifdef HEAP_CHECK
    size             = (size + 7) & ~7;
    size_t true_size = get_true_size(size);
    void  *ptr       = mpool_aligned_alloc(&pool, true_size, align);
    ptr              = set_magic(ptr, size, true);
#else
    void *ptr = mpool_aligned_alloc(&pool, size, align);
#endif
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

void init_heap() {
    uint64_t base_addr = KERNEL_HEAP_START;
    logkf("kernel_heap: init heap at %p - size: %llu\n", base_addr, KERNEL_HEAP_SIZE);
    page_map_range_to_random(get_kernel_pagedir(), base_addr, KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, (void *)base_addr, KERNEL_HEAP_SIZE);
}
