#include "fs/procfs.h"
#include "fs/vfs.h"
#include "string_builder.h"

char *proc_gen_filesystems(size_t *context_len) {
    string_builder_t *builder = create_string_builder(1024);
    vfs_filesystem_t  pos, n;
    bool              status = false;
    llist_for_each(pos, n, &fs_metadata_list, node) {
        if(pos->flags & FS_NO_MOUNT_FLAGS) continue;
        status &= string_builder_append(builder, "%s\t%s\n",
                                        (pos->flags & FS_VIRTUAL_FLAGS) ? "nodev" : "     ",
                                        pos->name);
    }
    *context_len = builder->size;
    char *data = builder->data;
    free(builder);
    return data;
}

size_t proc_filesystems_stat(proc_handle_t *handle) {
    size_t length = 0;
    free(proc_gen_filesystems(&length));
    return length;
}

size_t proc_filesystems_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size;
    char  *contect = proc_gen_filesystems(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, contect);
}
