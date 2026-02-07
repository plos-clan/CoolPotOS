#include "mem/heap.h"
#include "kasan.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/page.h"
#include "term/klog.h"

static void *heap_alloc(void *ptr, size_t size);

static struct mpool pool = {
    .cb_reqmem = heap_alloc,
};

static spin_t lock = SPIN_INIT;

#if KASAN_CHECK
static inline void kasan_mark_alloc(void *raw, void *user, size_t user_size) {
    if (raw == NULL || user == NULL) return;
    size_t block_size = mpool_msize(&pool, raw);
    kasan_poison(raw, block_size);
    kasan_unpoison(user, user_size);
}

static inline void kasan_mark_free(void *raw) {
    if (raw == NULL) return;
    size_t block_size = mpool_msize(&pool, raw);
    kasan_poison(raw, block_size);
}
#endif

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
#if KASAN_CHECK
    kasan_heap_extend((uintptr_t)ptr, size);
#endif
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
#if KASAN_CHECK
    kasan_push_disable();
#endif

#if HEAP_CHECK
    size             = (size + 7) & ~7;
    size_t true_size = get_true_size(size);
    void  *ptr       = mpool_alloc(&pool, true_size);
    if (!ptr) {
        logkf("\nkernel malloc null\n");
        arch_close_interrupt();
        arch_wait_for_interrupt();
    }
    ptr = set_magic(ptr, size, true);
#    if KASAN_CHECK
    kasan_pop_disable();
    kasan_mark_alloc((uint8_t *)ptr - sizeof(start_magic) - sizeof(size_t), ptr, size);
#    endif
#else
    void *ptr = mpool_alloc(&pool, size);
#endif

    alloc_exit(is_sti);
    return ptr;
}

void free(void *ptr) {
    if (!ptr) return;
    bool is_sti = alloc_enter();
#if KASAN_CHECK
    kasan_push_disable();
#endif
#if HEAP_CHECK
    ptr = check_magic(ptr, true);
#else
#endif
#if KASAN_CHECK
    kasan_pop_disable();
    kasan_mark_free(ptr);
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
#if KASAN_CHECK
    kasan_push_disable();
#endif
#if HEAP_CHECK
    void  *old_raw        = NULL;
    size_t old_block_size = 0;
    if (ptr != NULL) {
        old_raw        = check_magic(ptr, false);
        old_block_size = mpool_msize(&pool, old_raw);
    }
    newsize          = (newsize + 7) & ~7;
    size_t true_size = get_true_size(newsize);

    ptr = mpool_realloc(&pool, old_raw, true_size);
    ptr = set_magic(ptr, newsize, false);
#    if KASAN_CHECK
    kasan_pop_disable();
    if (old_raw && ptr != old_raw) kasan_poison(old_raw, old_block_size);
    kasan_mark_alloc((uint8_t *)ptr - sizeof(start_magic) - sizeof(size_t), ptr, newsize);
#    endif
#else
    void  *old_raw        = ptr;
    size_t old_block_size = (old_raw != NULL) ? mpool_msize(&pool, old_raw) : 0;
    ptr                   = mpool_realloc(&pool, ptr, newsize);
#endif
    alloc_exit(is_sti);
    return ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    const bool is_sti = alloc_enter();
#if KASAN_CHECK
    kasan_push_disable();
#endif
#if HEAP_CHECK
    size             = (size + 7) & ~7;
    size_t true_size = get_true_size(size);
    void  *ptr       = mpool_aligned_alloc(&pool, true_size, align);
    ptr              = set_magic(ptr, size, true);
#    if KASAN_CHECK
    kasan_pop_disable();
    kasan_mark_alloc((uint8_t *)ptr - sizeof(start_magic) - sizeof(size_t), ptr, size);
#    endif
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
#if KASAN_CHECK
    kasan_mark_alloc(ptr, ptr, size);
#endif
    alloc_exit(is_sti);
    return ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, alignment);
#if KASAN_CHECK
    kasan_mark_alloc(ptr, ptr, size);
#endif
    alloc_exit(is_sti);
    if (ptr == NULL) return 1;
    *memptr = ptr;
    return 0;
}

void *valloc(size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
#if KASAN_CHECK
    kasan_mark_alloc(ptr, ptr, size);
#endif
    alloc_exit(is_sti);
    return ptr;
}

void *pvalloc(size_t size) {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, PAGE_SIZE);
#if KASAN_CHECK
    kasan_mark_alloc(ptr, ptr, size);
#endif
    alloc_exit(is_sti);
    return ptr;
}

void init_heap() {
    uint64_t base_addr = KERNEL_HEAP_START;
    logkf("kernel_heap: init heap at %p - size: %llu\n", base_addr, KERNEL_HEAP_SIZE);
    page_map_range_to_random(get_kernel_pagedir(), base_addr, KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, (void *)base_addr, KERNEL_HEAP_SIZE);
    //heap_init((void *)base_addr, KERNEL_HEAP_SIZE);
#if KASAN_CHECK
    kasan_heap_init(base_addr, KERNEL_HEAP_SIZE);
#endif
}
