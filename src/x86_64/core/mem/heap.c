#include "heap.h"
#include "hhdm.h"
#include "krlibc.h"
#include "page.h"

uint64_t get_all_memusage() {
    return KERNEL_HEAP_SIZE;
}

/*
 * 跟智障的编译器优化斗智斗勇的产物
 */
void *calloc(size_t a, size_t b) {
    void *p = malloc(a * b);
    memset(p, 0, a * b);
    return p;
}

void *alloc_4k_aligned_mem(size_t size) {
    void *p        = malloc(size < PAGE_SIZE ? size + PAGE_SIZE : size);
    void *pAligned = (void *)((uint64_t)p + 0xFFF & ~0xFFF);
    return pAligned;
}

void init_heap() {
    page_map_range_to_random(get_kernel_pagedir(), (uint64_t)phys_to_virt(KERNEL_HEAP_START),
                             KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    heap_init(phys_to_virt(KERNEL_HEAP_START), KERNEL_HEAP_SIZE);
}
