#include "errno.h"
#include "fs/fds.h"
#include "fs/vfs.h"
#include "syscall.h"
#include "task/scheduler.h"
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

syscall_(write, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return EOK;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    size_t ret = vfs_write(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)-1) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) handle->offset += ret;
    vfs_update(handle->node);
    return ret;
}

syscall_(read, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return EOK;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    if(handle->node->type & file_pipe && handle->node->size == 0 && handle->flags & O_NONBLOCK){
        return SYSCALL_FAULT_(EWOULDBLOCK);
    }
    if (handle->node->size != (uint64_t)-1) {
        if (handle->offset >= handle->node->size) {
            if (handle->node->type & file_pipe) { goto pipe; }
            return EOK;
        }
    }
read:;
    size_t ret = vfs_read(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)-1) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) { handle->offset += ret; }
    return ret;
pipe:;
    while (handle->offset >= handle->node->size) {
        vfs_update(handle->node);
        scheduler_yield();
    }
    goto read;
}

syscall_(writev, int fd, struct iovec *iov, int iovcnt) {
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt == 0) return EOK;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    size_t          total  = 0;
    for (int i = 0; i < iovcnt; i++) {
        size_t status = vfs_write(handle->node, iov[i].iov_base, handle->offset, iov[i].iov_len);
        if (handle->node->size != (uint64_t)-1) {
            if (status == (size_t)-1) return total;
            handle->offset += status;
        }
        total += iov[i].iov_len;
    }
    return total;
}

syscall_(readv, int fd, struct iovec *iov, int iovcnt0) {
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt0 == 0) return EOK;
    size_t          iovcnt = iovcnt0;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (iovcnt == 0) return 0;
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    size_t buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf = (uint8_t *)malloc(buf_len);
    if (handle->node->size != (uint64_t)-1) {
        if (handle->offset > handle->node->size) return EOK;
    }
    size_t status = vfs_read(handle->node, buf, handle->offset, buf_len);
    if (status == (size_t)-1) {
        free(buf);
        return SYSCALL_FAULT_(EIO);
    }
    if (handle->node->size != (uint64_t)-1) handle->offset += status;
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        size_t len = iov[i].iov_len;
        if (len == 0) continue;

        size_t to_copy = len;
        if (copied + to_copy > status) { to_copy = status - copied; }

        memcpy(iov[i].iov_base, buf + copied, to_copy);
        copied += to_copy;
    }
    free(buf);
    return status;
}

