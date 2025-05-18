#include "alloc/area.h"
#include "heap.h"
#include "hhdm.h"
#include "krlibc.h"
#include "lock.h"
#include "page.h"
#include "scheduler.h"

#ifdef CPOS_HEAP_CHECK

static const size_t canaria[] = {
    0x1939e9d943699c68, 0xf03a1b212bd97bc8, 0x80bd66e7a068490b, 0x5e0d8f05b214ce1c,
    0xa4ba0cde51430b7a, 0x28b1e8e2840fb599, 0x25da605a676e0807, 0xed610c49c9f8db02,
};

static size_t *_malloc_rng(size_t *buf, size_t len) {
    static size_t seed = 0x0d0007210d000721;
    for (size_t i = 0; i < len; i++) {
        seed   = seed * 0x2e624114b6b255df + 0xb4f1e438a94f24a;
        buf[i] = seed + 0x290c2134746e4c5c;
    }
    return buf;
}

#    define PORT 0x3f8

static void _putb(int c) {
    while (!(io_in8(PORT + 5) & 0x20)) {}
    io_out8(PORT, c);
}

static void _puts(const char *s) {
    for (size_t i = 0; s[i] != '\0'; i++) {
        _putb(s[i]);
    }
}

static void _canaria_panic() {
    while (true) {
        __asm__ volatile("cli");
        _puts("\n"
              "===== ===== ===== ===== ===== ===== ===== =====\n"
              "Canaria panic: Memory corruption detected!\n"
              "Please report this issue to the developers.\n");
        __asm__ volatile("hlt");
    }
}

static void  *alloced_mem[8] = {};
static size_t nballocs       = 0;

#    define alloced_mem_entry                                                                      \
        (alloced_mem[nballocs++ % (sizeof(alloced_mem) / sizeof(*alloced_mem))])

size_t _check(const void *ptr) {
    const size_t *data = (const size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    const size_t len  = data[2] ^ canaria[2];
    const size_t len2 = data[3] ^ canaria[3];
    if (len != len2) _canaria_panic();
    const size_t pad  = data[len - 4] ^ canaria[4];
    const size_t pad2 = data[len - 3] ^ canaria[5];
    if (pad != pad2) _canaria_panic();
    if (pad & (pad - 1)) _canaria_panic();
    if (pad <= sizeof(size_t) * 2) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();
    return pad;
}

void _set(size_t *ptr, size_t len, size_t pad) {
    if (pad != 0) ptr += pad / sizeof(size_t) - 4;

    ptr[0]       = canaria[0];
    ptr[1]       = canaria[1];
    ptr[2]       = canaria[2] ^ len;
    ptr[3]       = canaria[3] ^ len;
    ptr[len - 4] = canaria[4] ^ pad;
    ptr[len - 3] = canaria[5] ^ pad;
    ptr[len - 2] = canaria[6];
    ptr[len - 1] = canaria[7];
}

static struct mpool pool;
static spin_t       lock = SPIN_INIT;

uint64_t get_all_memusage() {
    return KERNEL_HEAP_SIZE;
}

static bool alloc_enter() {
    const bool is_sti = get_rflags() & (1 << 9);
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

    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i]) _check(alloced_mem[i]);
    }
    size_t  len = (size + sizeof(size_t) - 1) / sizeof(size_t) + 8;
    size_t *ptr = mpool_alloc(&pool, len * sizeof(size_t));
    if (ptr == NULL) {
        alloc_exit(is_sti);
        return NULL;
    }
    _set(ptr, len, 0);
    void *addr = alloced_mem_entry = _malloc_rng(ptr + 4, len - 8);

    alloc_exit(is_sti);
    return addr;
}

void free(void *ptr) {
    const bool is_sti = alloc_enter();

    const size_t pad = _check(ptr);

    mpool_free(&pool, pad == 0 ? (size_t *)ptr - 4 : ptr - pad);
    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i] == ptr) {
            alloced_mem[i] = NULL;
            break;
        }
    }

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

void *realloc(void *ptr, size_t size) {
    const bool is_sti = alloc_enter();

    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i]) _check(alloced_mem[i]);
    }
    size_t *data = (size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    size_t len1 = data[2] ^ canaria[2];
    size_t len2 = data[3] ^ canaria[3];
    if (len1 != len2) _canaria_panic();
    size_t len = len1;
    if (data[len - 4] != (canaria[4] ^ len)) _canaria_panic();
    if (data[len - 3] != (canaria[5] ^ len)) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();
    size_t new_len = (size + sizeof(size_t) - 1) / sizeof(size_t) + 8;
    size_t *new    = mpool_realloc(&pool, data, new_len);
    if (new == NULL) {
        alloc_exit(is_sti);
        return NULL;
    }
    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i] == ptr) {
            alloced_mem[i] = NULL;
            break;
        }
    }
    new[0]           = canaria[0];
    new[1]           = canaria[1];
    new[2]           = canaria[2] ^ new_len;
    new[3]           = canaria[3] ^ new_len;
    new[new_len - 4] = canaria[4] ^ new_len;
    new[new_len - 3] = canaria[5] ^ new_len;
    new[new_len - 2] = canaria[6];
    new[new_len - 1] = canaria[7];
    if (new_len > len) _malloc_rng(new + len - 4, new_len - len);
    void *addr = alloced_mem_entry = new + 4;

    alloc_exit(is_sti);
    return addr;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) {
    if (align <= sizeof(size_t) * 2) return malloc(size);
    const bool is_sti = alloc_enter();

    size_t real_len = (size + sizeof(size_t) - 1) / sizeof(size_t);
    size_t len      = real_len + align * 2 / sizeof(size_t);
    void  *ptr      = mpool_aligned_alloc(&pool, len * sizeof(size_t), align);
    if (ptr == NULL) {
        alloc_exit(is_sti);
        return NULL;
    }

    _set(ptr, len, align);
    void *addr = alloced_mem_entry = _malloc_rng(ptr + align, real_len);

    alloc_exit(is_sti);
    return addr;
}

size_t malloc_usable_size(void *ptr) {
    const bool is_sti = alloc_enter();
    _check(ptr);
    size_t len = ((size_t *)ptr - 4)[2] ^ canaria[2];
    alloc_exit(is_sti);
    return (len - 8) * sizeof(size_t);
}

void init_heap() {
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)phys_to_virt(KERNEL_HEAP_START),
                             KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, phys_to_virt(KERNEL_HEAP_START), KERNEL_HEAP_SIZE);
}

#endif
