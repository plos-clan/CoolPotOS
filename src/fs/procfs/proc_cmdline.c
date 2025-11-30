#include "fs/procfs.h"
#include "bootarg.h"

size_t proc_cmdline_stat(proc_handle_t *handle){
    return strlen(get_kernel_cmdline());
}

size_t proc_cmdline_read(proc_handle_t *handle,void *addr, size_t offset, size_t size){
    size_t fs_size = strlen(get_kernel_cmdline());
    if (offset < fs_size) {
        if (size > fs_size) size = fs_size;
        memcpy(addr, get_kernel_cmdline() + offset, size);
        return size;
    } else
        return 0;
}
