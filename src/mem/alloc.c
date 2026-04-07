#include "krlibc.h"
#include <mem/alloc.h>
#include <lock.h>

spin_t heap_lock = SPIN_INIT;

void *malloc(size_t size) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(heap_lock);
    void *ptr = liballoc_malloc(size);
    spin_unlock(heap_lock);
    if (irq)
        arch_open_interrupt();
    return ptr;
}

void *calloc(size_t nmemb, size_t size) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(heap_lock);
    void *ptr = liballoc_calloc(nmemb, size);
    spin_unlock(heap_lock);
    if (irq)
        arch_open_interrupt();
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(heap_lock);
    void *nptr = liballoc_realloc(ptr, size);
    spin_unlock(heap_lock);
    if (irq)
        arch_open_interrupt();
    return nptr;
}

void *aligned_alloc(size_t alignment, size_t size) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(heap_lock);
    size      = PADDING_UP(size, alignment);
    void *ptr = liballoc_aligned_alloc(alignment, size);
    spin_unlock(heap_lock);
    if (irq)
        arch_open_interrupt();
    return ptr;
}

void free(void *ptr) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(heap_lock);
    liballoc_free(ptr);
    spin_unlock(heap_lock);
    if (irq)
        arch_open_interrupt();
}
