#include "boot.h"
#include "krlibc.h"
#include "limine.h"

USED SECTION(".limine_requests_start") static const volatile LIMINE_REQUESTS_START_MARKER;

USED SECTION(".limine_requests_end") static const volatile LIMINE_REQUESTS_END_MARKER;

LIMINE_REQUEST LIMINE_BASE_REVISION(2);

LIMINE_REQUEST struct limine_stack_size_request stack_request = {
    .id         = LIMINE_STACK_SIZE_REQUEST,
    .revision   = 0,
    .stack_size = KERNEL_ST_SZ // 128K
};

extern void kmain();

LIMINE_REQUEST struct limine_entry_point_request entry = {
    .id       = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 3,
    .entry    = &kmain,
};

LIMINE_REQUEST struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

LIMINE_REQUEST struct limine_hhdm_request hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

LIMINE_REQUEST struct limine_bootloader_info_request info_request = {
    .id       = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
};

LIMINE_REQUEST struct limine_smp_request smp_request = {
    .id       = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags    = LIMINE_SMP_X2APIC,
};

LIMINE_REQUEST struct limine_rsdp_request rsdp_request = {.id = LIMINE_RSDP_REQUEST, .revision = 0};

LIMINE_REQUEST struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

LIMINE_REQUEST struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
};

char *get_kernel_cmdline() {
    return cmdline_request.response->cmdline;
}

const char *get_bootloader_name() {
    struct limine_bootloader_info_response *limine_bootloader_request = info_request.response;

    if (limine_bootloader_request == NULL || limine_bootloader_request == NULL) {
        return "Unknown Bootloader";
    }
    return limine_bootloader_request->name;
}

const char *get_bootloader_version() {
    struct limine_bootloader_info_response *limine_bootloader_request = info_request.response;

    if (limine_bootloader_request == NULL || limine_bootloader_request == NULL) {
        return "Unknown Version";
    }
    return limine_bootloader_request->version;
}

uint64_t boot_physical_memory_offset() {
    return hhdm_request.response->offset;
}

struct limine_memmap_response *get_memory_map() {
    return memmap_request.response;
}

uint64_t get_memory_size() {
    uint64_t                       all_memory_size = 0;
    struct limine_memmap_response *memory_map      = memmap_request.response;

    for (uint64_t i = memory_map->entry_count - 1;; i--) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        if (region->type == LIMINE_MEMMAP_USABLE) {
            all_memory_size = region->base + region->length;
            break;
        }
    }
    return all_memory_size;
}

bool x2apic_mode_supported() {
    return !!(smp_request.response->flags & LIMINE_SMP_X2APIC);
}

struct limine_smp_response *get_smp_info() {
    return smp_request.response;
}

void *get_acpi_rsdp() {
    struct limine_rsdp_response *response = rsdp_request.response;
    return response->address;
}

struct limine_framebuffer_response *get_framebuffer_response() {
    return framebuffer_request.response;
}

struct limine_framebuffer *get_graphics_framebuffer() {
    struct limine_framebuffer_response *response = framebuffer_request.response;

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        return NULL;
    }

    return response->framebuffers[0];
}
