#include "boot.h"
#include "krlibc.h"

extern uint8_t _bss_start[], _bss_end[];
extern void kmain(void);

uint64_t boot_get_hhdm_offset() {
    return 0xfffffffef0000000;
}

boot_memory_map_t *boot_get_memory_map() {
    return NULL;
}

uintptr_t boot_get_acpi_rsdp() {
    return 0;
}

boot_framebuffer_t *boot_get_framebuffer(size_t index) {
    return NULL;
}

size_t boot_framebuffer_count() {
    return 0; // TODO
}

char *get_kernel_cmdline() {
    return "";
}

void boot_get_modules(boot_module_t **modules, size_t *count) {
    *count = 0;
}

uint64_t boot_get_dtb() {
    return 0; // TODO
}

_Noreturn void _boot_c_start() {
    memset(_bss_start, 0, (size_t)(_bss_end - _bss_start));

    kmain();

    for (;;)
        arch_wait_for_interrupt();
}
