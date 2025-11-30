#include "fs/procfs.h"
#include "fs/vfs.h"

const char filesystems_content[] = //"nodev\tsysfs\n"
    "nodev\ttmpfs\n"
    "nodev\tproc\n"
    "nodev\tmodfs\n"
    "     \tfatfs\n"
    "     \text4\n"
    "     \text3\n"
    "     \text2\n";

size_t proc_filesystems_stat(proc_handle_t *handle){
    return strlen(filesystems_content);
}

size_t proc_filesystems_read(proc_handle_t *handle,void *addr, size_t offset, size_t size){
    size_t fs_size = strlen(filesystems_content);
    if (offset < fs_size) {
        if (size > fs_size) size = fs_size;
        memcpy(addr, filesystems_content + offset, size);
        return size;
    } else
        return 0;
}
