#pragma once

#include "types.h"
#include "metadata.h"

typedef struct boot_memory_map_entry {
    uintptr_t addr;
    size_t len;
    enum {
        BOOT_MMAP_USABLE,   // 可用内存
        BOOT_MMAP_RESERVED, // 硬件保留
        BOOT_MMAP_KERNEL,   // 内核与模块装载区
        BOOT_MMAP_BAD,      // 已损坏的内存
    } type;
} boot_memory_map_entry_t;

typedef struct boot_memory_map {
    boot_memory_map_entry_t entries[8192];
    size_t entry_count;
} boot_memory_map_t;

typedef struct boot_framebuffer {
    uintptr_t address;
    size_t width;
    size_t height;
    size_t bpp;
    size_t pitch;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} boot_framebuffer_t;

typedef struct boot_module {
    char path[64];
    void *data;
    size_t size;
} boot_module_t;

size_t boot_get_hhdm_offset();
