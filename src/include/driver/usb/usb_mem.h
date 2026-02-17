#pragma once

#include "krlibc.h"
#include "mem/alloc/alloc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "types.h"

static inline void *usb_alloc_dma_pages(size_t pages, uint64_t *phys_out) {
    if (pages == 0) {
        if (phys_out) {
            *phys_out = 0;
        }
        return NULL;
    }

    size_t   size = pages * PAGE_SIZE;
    uint64_t phys = alloc_frames(pages);
    void    *virt = driver_phys_to_virt(phys);

    page_map_range(get_kernel_pagedir(), (uint64_t)virt, phys, size, KERNEL_PTE_FLAGS);
    if (virt) {
        memset(virt, 0, size);
        if (phys_out) {
            *phys_out = driver_virt_to_phys(virt);
        }
    }
    return virt;
}

static inline void usb_free_dma_pages(void *virt, size_t pages) {
    if (!virt || pages == 0) {
        return;
    }
    size_t size = pages * PAGE_SIZE;
    unmap_page_range(get_kernel_pagedir(), (uint64_t)virt, size);
}
