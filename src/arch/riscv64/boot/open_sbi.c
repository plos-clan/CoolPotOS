#include "boot.h"

extern uintptr_t smp_entry;

boot_memory_map_t opensbi_memory_map;
boot_framebuffer_t opensbi_fb;
static char empty_kernel_cmdline[] = "";
char *kernel_cmdline = empty_kernel_cmdline;
extern uintptr_t opensbi_dtb_vaddr;

uint64_t boot_get_hhdm_offset() {
    return 0xffff800000000000;
}

boot_memory_map_t *boot_get_memory_map() {
    return &opensbi_memory_map;
}

uintptr_t boot_get_acpi_rsdp() {
    return 0;
}

boot_framebuffer_t *boot_get_framebuffer(size_t index) {
    return &opensbi_fb;
}

size_t boot_framebuffer_count() {
    return 0; // TODO
}

char *get_kernel_cmdline() {
    return kernel_cmdline ? kernel_cmdline : empty_kernel_cmdline;
}

boot_module_t opensbi_modules[MAX_LOAD_MODULE];

void boot_get_modules(boot_module_t **modules, size_t *count) {
    for (uint64_t i = 0; i < 1; i++) {
        modules[i] = &opensbi_modules[i];
        (*count)++;
    }
}

uint64_t boot_get_dtb() {
    return opensbi_dtb_vaddr;
}
