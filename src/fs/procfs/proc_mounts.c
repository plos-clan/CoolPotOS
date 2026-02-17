#include "fs/procfs.h"

char *mount_info =
    "dev /dev devfs rw,nosuid,relatime,mode=755,inode64 0 0\n"
    "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n"
    "tmpfs /tmp tmpfs rw,nosuid,size=8040232k,nr_inodes=1048576,nodev,inode64,usrquota 0 0\n";

size_t proc_mounts_stat(proc_handle_t *handle) {
    return strlen(mount_info);
}

size_t proc_mounts_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size = strlen(mount_info);
    if (offset < fs_size) {
        if (size > fs_size)
            size = fs_size;
        memcpy(addr, mount_info + offset, size);
        return size;
    } else
        return 0;
}
