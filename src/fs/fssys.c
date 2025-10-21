#include "errno.h"
#include "fs/fds.h"
#include "fs/vfs.h"
#include "syscall.h"
#include "task/task.h"
#include "term/klog.h"

syscall_(open, char *path0, uint64_t flags, uint64_t mode) {
    if (unlikely(path0 == NULL)) return SYSCALL_FAULT_(EINVAL);

    char *normalized_path = vfs_cwd_path_build(path0);

    logkf("sys_open: open %s\n", normalized_path);

    vfs_node_t node = vfs_open(normalized_path);
    if (node == NULL) {
        if (flags & O_CREAT) {
            if (mode & O_DIRECTORY) {
                vfs_mkdir(normalized_path);
            } else
                vfs_mkfile(normalized_path);
            node = vfs_open(normalized_path);
            if (node == NULL)
                goto err;
            else
                goto next;
        } else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
next:;
    fd_t *fd_handle = calloc(1, sizeof(fd_t));
    not_null_assert(fd_handle, "sys_open: null alloc fd");
    fd_handle->offset = flags & O_APPEND ? node->size : 0;
    fd_handle->node   = node;
    int index         = add_fd(get_current_task()->process->fdts, fd_handle);
    fd_handle->fd     = index;
    if (index == -1) {
        logkf("sys_open: open %s failed.\n", normalized_path);
        vfs_close(node);
        free(fd_handle);
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(normalized_path);
    return index;
}

syscall_(close, int fd) {
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = (fd_t *)get_fd(get_current_task()->process->fdts,fd);
    vfs_close(handle->node);
    free(handle);
    return EOK;
}
