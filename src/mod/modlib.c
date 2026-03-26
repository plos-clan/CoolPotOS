#include "driver/blk_device.h"
#include "driver/pci/pci.h"
#include "exec/dlinker.h"
#include "lib/sprintf.h"
#include "mem/frame.h"
#include "mem/page.h"

#define EXPORT_SYMBOL(mode, FUNC) dlfunc_register(mode, #FUNC, (void *)(FUNC))

static void cp_printk(const char *fmt, ...) {
    char buf[4096] = { 0 };
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    tty_t *tty_ = get_kernel_session();
    tty_->ops.write(tty_, buf, 0, strlen(buf));
    tty_->ops.flush(tty_);
}

static uint64_t get_kernel_pte_flags() {
    return KERNEL_PTE_FLAGS;
}

static void register_cp_kernel_lib(kernel_mode_t *kernel) {
    dlfunc_register(kernel, "printk", cp_printk);
    dlfunc_register(kernel, "bcmp", memcmp);
    EXPORT_SYMBOL(kernel, memset);
    EXPORT_SYMBOL(kernel, memmove);
    EXPORT_SYMBOL(kernel, memchr);
    EXPORT_SYMBOL(kernel, memcmp);
    EXPORT_SYMBOL(kernel, memcpy);
    EXPORT_SYMBOL(kernel, strnlen);
    EXPORT_SYMBOL(kernel, strlen);
    EXPORT_SYMBOL(kernel, strcat);
    EXPORT_SYMBOL(kernel, strcpy);
    EXPORT_SYMBOL(kernel, strncpy);
    EXPORT_SYMBOL(kernel, strchrnul);
    EXPORT_SYMBOL(kernel, strncmp);
    EXPORT_SYMBOL(kernel, strchr);
    EXPORT_SYMBOL(kernel, strcmp);
    EXPORT_SYMBOL(kernel, strrchr);
    EXPORT_SYMBOL(kernel, strtok);
    EXPORT_SYMBOL(kernel, strdup);
    EXPORT_SYMBOL(kernel, strndup);
    EXPORT_SYMBOL(kernel, strtol);
    EXPORT_SYMBOL(kernel, sprintf);
    EXPORT_SYMBOL(kernel, snprintf);
    EXPORT_SYMBOL(kernel, vsnprintf);
    EXPORT_SYMBOL(kernel, malloc);
    EXPORT_SYMBOL(kernel, calloc);
    EXPORT_SYMBOL(kernel, realloc);
    EXPORT_SYMBOL(kernel, free);
    EXPORT_SYMBOL(kernel, qsort);
    EXPORT_SYMBOL(kernel, arch_pause);
}

static void register_fs_subsystem_lib(kernel_mode_t *kernel) {
    EXPORT_SYMBOL(kernel, vfs_mkdir);
    EXPORT_SYMBOL(kernel, vfs_mkfile);
    EXPORT_SYMBOL(kernel, vfs_regist);
    EXPORT_SYMBOL(kernel, vfs_link);
    EXPORT_SYMBOL(kernel, vfs_symlink);
    EXPORT_SYMBOL(kernel, vfs_child_append);
    EXPORT_SYMBOL(kernel, vfs_node_alloc);
    EXPORT_SYMBOL(kernel, vfs_close);
    EXPORT_SYMBOL(kernel, vfs_free);
    EXPORT_SYMBOL(kernel, vfs_update);
    EXPORT_SYMBOL(kernel, vfs_open);
    EXPORT_SYMBOL(kernel, vfs_ioctl);
    EXPORT_SYMBOL(kernel, vfs_readlink);
    EXPORT_SYMBOL(kernel, get_filesystem);
    EXPORT_SYMBOL(kernel, get_filesystem_node);
    EXPORT_SYMBOL(kernel, get_rootdir);
    EXPORT_SYMBOL(kernel, set_rootdir);
    EXPORT_SYMBOL(kernel, vfs_get_fullpath);
    EXPORT_SYMBOL(kernel, vfs_read);
    EXPORT_SYMBOL(kernel, vfs_write);
    EXPORT_SYMBOL(kernel, vfs_mount);
    EXPORT_SYMBOL(kernel, vfs_unmount);
    EXPORT_SYMBOL(kernel, vfs_poll);
    EXPORT_SYMBOL(kernel, vfs_rename);
    EXPORT_SYMBOL(kernel, general_map);
    EXPORT_SYMBOL(kernel, vfs_free_child);
    EXPORT_SYMBOL(kernel, vfs_delete);
}

static void register_mem_subsystem_lib(kernel_mode_t *kernel) {
    EXPORT_SYMBOL(kernel, alloc_frames);
    EXPORT_SYMBOL(kernel, free_frames);
    EXPORT_SYMBOL(kernel, free_frame);
    EXPORT_SYMBOL(kernel, driver_phys_to_virt);
    EXPORT_SYMBOL(kernel, driver_virt_to_phys);
    EXPORT_SYMBOL(kernel, get_kernel_pagedir);
    EXPORT_SYMBOL(kernel, get_kernel_pte_flags);
    EXPORT_SYMBOL(kernel, page_map_range);
    EXPORT_SYMBOL(kernel, unmap_page_range);
    EXPORT_SYMBOL(kernel, arch_virt_to_phys);
    EXPORT_SYMBOL(kernel, phys_to_virt);
}

static void register_driver_subsystem_lib(kernel_mode_t *kernel) {
    EXPORT_SYMBOL(kernel, nano_time);
    EXPORT_SYMBOL(kernel, pci_find_class);
    EXPORT_SYMBOL(kernel, register_device);
}

void load_all_kmod_lib(kernel_mode_t *kernel) {
    register_cp_kernel_lib(kernel);
    register_fs_subsystem_lib(kernel);
    register_mem_subsystem_lib(kernel);
    register_driver_subsystem_lib(kernel);
}
