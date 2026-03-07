#include "boot.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"

extern uint64_t fdt_get_initrd(const void *fdt, size_t *out_size);
extern void *opensbi_dtb_vaddr;
extern boot_module_t opensbi_modules[MAX_LOAD_MODULE];

static _Atomic(bool) has_initrd = false;

bool is_initrd() {
    return has_initrd;
}

void initrd_setup() {
    size_t size;
    const uint64_t buffer = fdt_get_initrd(opensbi_dtb_vaddr, &size);
    if (buffer == 0) {
        kerror("cannot find initrd file.");
        return;
    }
    opensbi_modules[0].size = size;
    opensbi_modules[0].data = phys_to_virt(buffer);
    page_map_range(
        get_kernel_pagedir(), (uint64_t)opensbi_modules[0].data, buffer, size, KERNEL_PTE_FLAGS
    );

    strcpy(opensbi_modules[0].name, "initramfs");
    strcpy(opensbi_modules[0].path, "/initramfs.img");
    has_initrd = true;
}
