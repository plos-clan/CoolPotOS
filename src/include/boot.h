#pragma once

#include "metadata.h"
#include "types.h"

typedef struct boot_framebuffer {
    uintptr_t address;
    size_t    width;
    size_t    height;
    size_t    bpp;
    size_t    pitch;
    uint8_t   red_mask_size;
    uint8_t   red_mask_shift;
    uint8_t   green_mask_size;
    uint8_t   green_mask_shift;
    uint8_t   blue_mask_size;
    uint8_t   blue_mask_shift;
} boot_framebuffer_t;

typedef struct boot_memory_map_entry {
    uintptr_t base;
    size_t    length;
    enum {
        BOOT_MMAP_USABLE,
        BOOT_MMAP_RESERVED,
        BOOT_MMAP_ACPI_RECLAIMABLE,
        BOOT_MMAP_ACPI_NVS,
        BOOT_MMAP_BAD,
        BOOT_MMAP_BOOTLOADER_RECLAIMABLE,
        BOOT_MMAP_KERNEL_MODULE,          // for API rev < 2
        BOOT_MMAP_EXECUTABLE_AND_MODULES, // for API rev >= 2
        BOOT_MMAP_FRAMEBUFFER,
    } type;
} boot_memory_map_entry_t;

typedef struct boot_memory_map {
    boot_memory_map_entry_t entries[8192];
    size_t                  entry_count;
} boot_memory_map_t;

typedef struct boot_module {
    char   path[64];
    char   name[64];
    void  *data;
    size_t size;
} boot_module_t;

uintptr_t           boot_get_acpi_rsdp();
void                boot_get_modules(boot_module_t **modules, size_t *count);
size_t              boot_framebuffer_count();
boot_framebuffer_t *boot_get_framebuffer(size_t index);
uint64_t            boot_get_hhdm_offset();
boot_memory_map_t  *boot_get_memory_map();
char               *get_kernel_cmdline();
uint64_t            boot_get_dtb();
