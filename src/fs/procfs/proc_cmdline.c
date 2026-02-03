#include "bootarg.h"
#include "fs/procfs.h"

size_t proc_cmdline_stat(proc_handle_t *handle) {
    return strlen(get_kernel_cmdline()) + 1;
}

size_t proc_cmdline_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t cmd_len = strlen(get_kernel_cmdline());
    size_t fs_size = cmd_len + 1;
    if (offset < fs_size) {
        if (size > fs_size) size = fs_size;
        if (offset < cmd_len) {
            size_t copy_len = size;
            if (offset + copy_len > cmd_len) {
                copy_len = cmd_len - offset;
            }
            memcpy(addr, get_kernel_cmdline() + offset, copy_len);
            if (copy_len < size) {
                ((char *)addr)[copy_len] = '\n';
            }
        } else if (offset == cmd_len) {
            ((char *)addr)[0] = '\n';
        }
        return size;
    } else
        return 0;
}
