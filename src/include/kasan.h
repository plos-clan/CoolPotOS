#pragma once

#include "metadata.h"
#include "types.h"

#if defined(KASAN_CHECK) && KASAN_CHECK && (defined(__x86_64__) || defined(__amd64__))
void kasan_init(void);
void kasan_heap_init(uintptr_t heap_start, size_t heap_size);
void kasan_heap_extend(uintptr_t heap_start, size_t heap_size);
void kasan_poison(const void *addr, size_t size);
void kasan_unpoison(const void *addr, size_t size);
void kasan_check_range(const void *addr, size_t size, bool is_write, const char *reason);
bool kasan_is_active(void);
void kasan_push_disable(void);
void kasan_pop_disable(void);

static inline void kasan_check_read(const void *addr, size_t size) {
    kasan_check_range(addr, size, false, __func__);
}

static inline void kasan_check_write(const void *addr, size_t size) {
    kasan_check_range(addr, size, true, __func__);
}
#endif
