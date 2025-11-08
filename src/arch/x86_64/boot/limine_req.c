#include "boot.h"
#include "krlibc.h"
#include "limine.h"
#include "task/scheduler.h"
#include "task/smp.h"

USED SECTION(".limine_requests_start") static volatile LIMINE_REQUESTS_START_MARKER;
USED SECTION(".limine_requests_end") static const volatile LIMINE_REQUESTS_END_MARKER;

LIMINE_REQUEST LIMINE_BASE_REVISION(3);

LIMINE_REQUEST struct limine_stack_size_request stack_request = {
    .id         = LIMINE_STACK_SIZE_REQUEST,
    .revision   = 0,
    .stack_size = MAX_STACK_SIZE // 128K
};

LIMINE_REQUEST struct limine_hhdm_request hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

LIMINE_REQUEST struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

LIMINE_REQUEST struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
};

LIMINE_REQUEST struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

LIMINE_REQUEST struct limine_module_request modules_request = {
    .id       = LIMINE_MODULE_REQUEST,
    .revision = 0,
};

LIMINE_REQUEST struct limine_rsdp_request rsdp_request = {.id = LIMINE_RSDP_REQUEST, .revision = 0};

uint64_t boot_get_hhdm_offset() {
    return hhdm_request.response->offset;
}

boot_memory_map_t limine_boot_memory_map;

boot_memory_map_t *boot_get_memory_map() {
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *le = memmap_request.response->entries[i];
        int                         mapped_type;
        switch (le->type) {
        case LIMINE_MEMMAP_USABLE: mapped_type = BOOT_MMAP_USABLE; break;
        case LIMINE_MEMMAP_RESERVED: mapped_type = BOOT_MMAP_RESERVED; break;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE: mapped_type = BOOT_MMAP_ACPI_RECLAIMABLE; break;
        case LIMINE_MEMMAP_ACPI_NVS: mapped_type = BOOT_MMAP_ACPI_NVS; break;
        case LIMINE_MEMMAP_BAD_MEMORY: mapped_type = BOOT_MMAP_BAD; break;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            mapped_type = BOOT_MMAP_BOOTLOADER_RECLAIMABLE;
            break;
        case LIMINE_MEMMAP_FRAMEBUFFER: mapped_type = BOOT_MMAP_FRAMEBUFFER; break;
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            mapped_type = BOOT_MMAP_EXECUTABLE_AND_MODULES;
            break;
        default: mapped_type = BOOT_MMAP_RESERVED; break;
        }
        limine_boot_memory_map.entries[i] = (boot_memory_map_entry_t){
            .base   = le->base,
            .length = le->length,
            .type   = mapped_type,
        };
    }

    limine_boot_memory_map.entry_count = memmap_request.response->entry_count;
    return &limine_boot_memory_map;
}

char *get_kernel_cmdline() {
    return cmdline_request.response->cmdline;
}

size_t boot_framebuffer_count() {
    return framebuffer_request.response->framebuffer_count;
}

boot_framebuffer_t limine_boot_fb[MAX_FRAMEBUFFER];

boot_framebuffer_t *boot_get_framebuffer(size_t index) {
    limine_boot_fb[index].address =
        (uintptr_t)framebuffer_request.response->framebuffers[0]->address;
    limine_boot_fb[index].width  = framebuffer_request.response->framebuffers[0]->width;
    limine_boot_fb[index].height = framebuffer_request.response->framebuffers[0]->height;
    limine_boot_fb[index].bpp    = framebuffer_request.response->framebuffers[0]->bpp;
    limine_boot_fb[index].pitch  = framebuffer_request.response->framebuffers[0]->pitch;
    limine_boot_fb[index].red_mask_shift =
        framebuffer_request.response->framebuffers[0]->red_mask_shift;
    limine_boot_fb[index].red_mask_size =
        framebuffer_request.response->framebuffers[0]->red_mask_size;
    limine_boot_fb[index].blue_mask_shift =
        framebuffer_request.response->framebuffers[0]->blue_mask_shift;
    limine_boot_fb[index].blue_mask_size =
        framebuffer_request.response->framebuffers[0]->blue_mask_size;
    limine_boot_fb[index].green_mask_shift =
        framebuffer_request.response->framebuffers[0]->green_mask_shift;
    limine_boot_fb[index].green_mask_size =
        framebuffer_request.response->framebuffers[0]->green_mask_size;

    return &limine_boot_fb[index];
}

boot_module_t limine_boot_modules[MAX_LOAD_MODULE];

void boot_get_modules(boot_module_t **modules, size_t *count) {
    *count = 0;
    for (uint64_t i = 0; i < modules_request.response->module_count; i++) {
        strcpy(limine_boot_modules[i].path, modules_request.response->modules[i]->path);
        limine_boot_modules[i].data = modules_request.response->modules[i]->address;
        limine_boot_modules[i].size = modules_request.response->modules[i]->size;
        modules[i]                  = &limine_boot_modules[i];
        (*count)++;
    }
}

uintptr_t boot_get_acpi_rsdp() {
    return (uintptr_t)rsdp_request.response->address;
}

LIMINE_REQUEST struct limine_smp_request mp_request = {
    .id       = LIMINE_SMP_REQUEST,
    .revision = 0,
#if defined(__x86_64__) || defined(__amd64__)
    .flags = LIMINE_SMP_X2APIC,
#endif
};

#if defined(__x86_64__) || defined(__amd64__)
bool x2apic_mode_supported() {
    return !!(mp_request.response->flags & LIMINE_SMP_X2APIC);
}
#endif

void smp_cpu_init(uint64_t *cpu_count, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos) {
    struct limine_smp_response *mp_response = mp_request.response;
    *cpu_count                              = mp_response->cpu_count;

    for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
        struct limine_smp_info *cpu = mp_response->cpus[i];
        cpu_local_infos[i].enable   = true;
        cpu_local_infos[i].id       = cpu->lapic_id;
        *bsp_cpu_id                 = mp_response->bsp_lapic_id;
        if (cpu->lapic_id == mp_response->bsp_lapic_id) {
            set_bsp_cpu_info(&cpu_local_infos[i]);
            continue;
        }
        cpu->goto_address = (limine_goto_address)arch_ap_cpu_entry;
    }
}

uint64_t boot_get_dtb() {
    return 0;
}
