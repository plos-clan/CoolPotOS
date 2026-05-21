#include "limine.h"
#include "boot.h"

#define LIMINE_REQUEST __attribute__((used, section(".limine_requests"))) static volatile

__attribute__((
    used,
    section(
        ".limine_requests_"
        "start"
    )
)) static volatile uint64_t requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

LIMINE_REQUEST uint64_t base_revision_request[4] = LIMINE_BASE_REVISION(6);

LIMINE_REQUEST struct limine_stack_size_request stack_size_request = {
    .id         = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision   = 0,
    .stack_size = KERNEL_STACK_SIZE,
};

LIMINE_REQUEST struct limine_hhdm_request hhdm_request = { .id       = LIMINE_HHDM_REQUEST_ID,
                                                           .revision = 0 };

LIMINE_REQUEST struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

__attribute__((
    used, section(".limine_requests_end")
)) static volatile uint64_t requests_end_marker[4] = LIMINE_REQUESTS_END_MARKER;

boot_memory_map_t limine_boot_memory_map;

static int limine_to_memory_type(uint64_t limine_type) {
    switch (limine_type) {
    case LIMINE_MEMMAP_USABLE:
        return BOOT_MMAP_USABLE;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return BOOT_MMAP_BAD;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return BOOT_MMAP_KERNEL;
    default:
        return BOOT_MMAP_RESERVED;
    }
}

boot_memory_map_t *boot_get_memory_map() {
    size_t entry_count = 0;
    for (entry_count = 0; entry_count < memmap_request.response->entry_count; entry_count++) {
        struct limine_memmap_entry *limine_entry    = memmap_request.response->entries[entry_count];
        limine_boot_memory_map.entries[entry_count] = (boot_memory_map_entry_t){
            .addr = limine_entry->base,
            .len  = limine_entry->length,
            .type = limine_to_memory_type(limine_entry->type),
        };
    }
    limine_boot_memory_map.entry_count = entry_count;
    return &limine_boot_memory_map;
}

size_t boot_get_hhdm_offset() {
    return hhdm_request.response->offset;
}
