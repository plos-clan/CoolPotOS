#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/fds.h"
#include "fs/pipefs.h"
#include "fs/vfs.h"
#include "syscall.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

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
    fd_t *handle = (fd_t *)get_fd(get_current_task()->process->fdts, fd);
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
    if (handle->node->type & file_pipe && handle->node->size == 0 && handle->flags & O_NONBLOCK) {
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
    fd_t  *handle = get_fd(get_current_task()->process->fdts, fd);
    size_t total  = 0;
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
    size_t iovcnt = iovcnt0;
    fd_t  *handle = get_fd(get_current_task()->process->fdts, fd);
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

syscall_(stat, char *fn, struct stat *buf) {
    if (unlikely(fn == NULL || buf == NULL)) return SYSCALL_FAULT_(EINVAL);
    char      *path = vfs_cwd_path_build(fn);
    vfs_node_t node = vfs_open(path);
    if (node == NULL) {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    buf->st_gid   = (int)node->group;
    buf->st_uid   = (int)node->owner;
    buf->st_ino   = node->inode;
    buf->st_size  = (long long int)node->size;
    buf->st_mode  = node->mode | (node->type == file_symlink  ? S_IFLNK
                                  : node->type == file_dir    ? S_IFDIR
                                  : node->type == file_block  ? S_IFBLK
                                  : node->type == file_socket ? S_IFSOCK
                                  : node->type == file_none   ? S_IFREG
                                  : node->type == file_stream ? S_IFCHR
                                                              : S_IFREG);
    buf->st_nlink = 1;
    buf->st_dev   = (long)node->dev;
    buf->st_rdev  = (long)node->rdev;
    free(path);
    return EOK;
}

syscall_(ioctl, int fd, int options, void *arg2) {
    if (unlikely(fd < 0 || arg2 == NULL)) return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    return vfs_ioctl(handle->node, options, arg2);
}

fd_t *fd_dup(fd_t *src) {
    fd_t *new = (fd_t *)malloc(sizeof(fd_t));
    not_null_assert(new, "fd_dup out of memory.");
    src->node->refcount++;
    new->node       = src->node;
    new->offset     = src->offset;
    new->flags      = src->flags;
    new->fd         = src->fd;
    vfs_node_t node = new->node;
    if (node->type == file_pipe) {
        pipe_specific_t *spec = node->handle;
        pipe_info_t     *pipe = spec->info;
        if (spec->write) {
            pipe->write_fds++;
        } else {
            pipe->read_fds++;
        }
    }
    return new;
}

syscall_(dup2, int fd, int newfd) {
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

    fd_t *old_handle = get_fd(get_current_task()->process->fdts, newfd);
    if (old_handle != NULL) {
        remove_fd(get_current_task()->process->fdts, newfd);
        vfs_close(old_handle->node);
        free(old_handle);
    }

    fd_t *new_handle = fd_dup(handle);
    if (new_handle == NULL) return SYSCALL_FAULT_(ENOMEM);
    new_handle->fd = newfd;
    set_fd(get_current_task()->process->fdts, new_handle, newfd);
    return newfd;
}

syscall_(dup, int fd) {
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    fd_t *new_handle      = fd_dup(handle);
    return new_handle->fd = add_fd(get_current_task()->process->fdts, new_handle);
}

syscall_(getcwd, char *buffer, size_t length) {
    if (unlikely(buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(length == 0)) return EOK;
    pcb_t  process  = get_current_task()->process;
    char  *cwd      = vfs_get_fullpath(process->cwd);
    size_t cwd_leng = strlen(cwd);
    if (length > cwd_leng) length = cwd_leng;
    memcpy(buffer, cwd, length);
    return length;
}

syscall_(chdir, char *s) {
    if (unlikely(s == NULL)) return SYSCALL_FAULT_(EINVAL);
    pcb_t process = get_current_task()->process;

    char *path;
    char *bpath = NULL;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        bpath = vfs_get_fullpath(process->cwd);
        path  = pathacat(bpath, s);
    }

    char *normalized_path = normalize_path(path);
    free(path);
    free(bpath);

    if (unlikely(normalized_path == NULL)) { return SYSCALL_FAULT_(ENOMEM); }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL) {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (node->type == file_dir) {
        process->cwd = node;
    } else {
        return SYSCALL_FAULT_(ENOTDIR);
    }

    free(normalized_path);
    return EOK;
}

syscall_(fcntl, int fd, int cmd, uint64_t arg) {
    if (fd < 0 || cmd < 0) return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    switch (cmd) {
    case F_GETFD: return (handle->node->flags & O_CLOEXEC) != 0;
    case F_SETFD: return handle->node->flags |= O_CLOEXEC;
    case F_DUPFD_CLOEXEC:;
        uint64_t newfd       = syscall_dup(fd, 0, 0, 0, 0, 0, regs);
        handle->node->flags |= O_CLOEXEC;
        return newfd;
    case F_DUPFD: return syscall_dup(fd, 0, 0, 0, 0, 0, regs);
    case F_GETFL: return handle->node->flags;
    case F_SETFL:;
        uint32_t valid_flags  = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        handle->node->flags  &= ~valid_flags;
        handle->node->flags  |= arg & valid_flags;
    default: break;
    }
    return EOK;
}

syscall_(mount, char *dev_name, char *dir_name, char *type, uint64_t flags, void *data) {
    if (dir_name == NULL) return SYSCALL_FAULT_(EINVAL);

    char      *ndir_name = vfs_cwd_path_build(dir_name);
    vfs_node_t dir       = vfs_open((const char *)ndir_name);
    if (!dir) {
        free(ndir_name);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (flags & MS_MOVE) {
        if (flags & (MS_REMOUNT | MS_BIND)) {
            free(ndir_name);
            return SYSCALL_FAULT_(EINVAL);
        }
        char      *old_root_p = vfs_cwd_path_build(dev_name);
        vfs_node_t old_root   = vfs_open(old_root_p);
        free(old_root_p);
        if (old_root == NULL || !old_root->is_mount) return SYSCALL_FAULT_(EINVAL);
        if (dir != rootdir) list_append(dir->parent->child, old_root);
        char *nb       = old_root->name;
        old_root->name = dir->name;
        dir->name      = nb;
        list_append(old_root->parent->child, dir);

        list_delete(old_root->parent->child, old_root);
        if (dir != rootdir)
            list_delete(dir->parent->child, dir);
        else
            rootdir = old_root;

        vfs_node_t parent = dir->parent;
        dir->parent       = old_root->parent;
        old_root->parent  = parent;

        vfs_close(old_root);
        vfs_close(dir);
        return EOK;
    }

    if (type == NULL) return SYSCALL_FAULT_(EINVAL);

    char *ndev_name = vfs_cwd_path_build(dev_name);
mount:
    if (vfs_mount((const char *)ndev_name, type, dir) != EOK) {
        free(ndir_name);
        free(ndev_name);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(ndir_name);
    free(ndev_name);
    return EOK;
}

syscall_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout) {
    int      ready      = 0;
    uint64_t start_time = nano_time();
    bool     sigexit    = false;

    extern vfs_callback_t fs_callbacks[256];

    do {
        // 检查每个文件描述符
        for (size_t i = 0; i < get_current_task()->process->fdts->fds_length; i++) {
            fd_t      *handle = get_current_task()->process->fdts->fds[i];
            vfs_node_t node   = handle->node;
            if (fs_callbacks[node->fsid]->poll == (void *)dummy) {
                if (fds_user[i].events & POLLIN || fds_user[i].events & POLLOUT) {
                    fds_user[i].revents = fds_user[i].events & POLLIN ? POLLIN : POLLOUT;
                    ready++;
                }
                i++;
                continue;
            }
            int revents =
                (int)epoll_to_poll_comp(vfs_poll(node, poll_to_epoll_comp(fds_user[i].events)));
            if (revents > 0) {
                fds_user[i].revents = (short)revents;
                ready++;
            }
        }

        // sigexit = signals_pending_quick(current_task);

        if (ready > 0 || sigexit) break;

        arch_open_interrupt();
        arch_pause();
    } while (timeout != 0 && ((int)timeout == -1 || (nano_time() - start_time) < timeout));

    arch_close_interrupt();
    if (!ready && sigexit) return (size_t)-EINTR;
    return ready;
}

syscall_(fstat, int fd, struct stat *buf) {
    if (unlikely(buf == NULL)) return SYSCALL_FAULT_(EINVAL);

    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);
    vfs_node_t node = handle->node;
    buf->st_gid     = (int)node->group;
    buf->st_uid     = (int)node->owner;
    buf->st_size    = node->size == (uint64_t)-1 ? 0 : (long long int)node->size;
    buf->st_mode    = node->type | (node->type == file_symlink  ? S_IFLNK
                                    : node->type == file_dir    ? S_IFDIR
                                    : node->type == file_block  ? S_IFBLK
                                    : node->type == file_socket ? S_IFSOCK
                                    : node->type == file_none   ? S_IFREG
                                    : node->type == file_stream ? S_IFCHR
                                                                : 0);
    buf->st_nlink   = 1;
    buf->st_dev     = node->dev;
    buf->st_rdev    = node->rdev;
    buf->st_ctim = buf->st_atim = buf->st_ctim = buf->st_mtim = (struct timespec){
        .tv_sec = node->createtime / 1000000000ULL, .tv_nsec = node->createtime % 1000000000ULL};
    buf->st_blksize = PAGE_SIZE;
    buf->st_blocks  = (node->size + PAGE_SIZE - 1) / PAGE_SIZE;
    buf->st_ino     = node->inode;
    return EOK;
}
