#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/fds.h"
#include "fs/pipefs.h"
#include "fs/vfs.h"
#include "mem/frame.h"
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
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
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
        for (size_t i = 0; i < nfds; i++) {
            fd_t *handle = get_fd(get_current_task()->process->fdts,fds_user[i].fd);
            if (handle == NULL) {
                fds_user[i].revents |= POLLNVAL;
                continue;
            }
            vfs_node_t node = handle->node;
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

syscall_(umount2, char *path0) {
    char   *path   = normalize_path(path0);
    int     flags  = arg1;
    errno_t status = vfs_unmount(path);
    long    ret    = status;
    if (status != EOK) ret = SYSCALL_FAULT_(EBUSY);
    free(path);
    return ret;
}

syscall_(lseek, int fd, size_t offset, size_t whence) {
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

    int64_t real_offset = (int64_t)offset;
    if (real_offset < 0 && handle->node->type & file_none && whence != SEEK_CUR)
        return SYSCALL_FAULT_(EBADF);
    switch (whence) {
    case SEEK_SET: handle->offset = real_offset; break;
    case SEEK_CUR:
        handle->offset += real_offset;
        if ((int64_t)handle->offset < 0) {
            handle->offset = 0;
        } else if (handle->offset > handle->node->size) {
            handle->offset = handle->node->size;
        }
        break;
    case SEEK_END: handle->offset = handle->node->size - real_offset; break;
    case SEEK_DATA:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        break;
    case SEEK_HOLE:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        return handle->node->size;
    default: return SYSCALL_FAULT_(ENXIO);
    }

    return handle->offset;
}

syscall_(pread, int fd, uint8_t *buffer) {
    syscall_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return syscall_read(fd, buffer, arg2, 0, 0, 0, regs);
}

syscall_(pwrite, int fd, uint8_t *buffer) {
    syscall_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return syscall_write(fd, buffer, arg2, 0, 0, 0, regs);
}

syscall_(copy_file_range, int fd_in, uint64_t *off_in, int fd_out, uint64_t *off_out, size_t len,
         uint64_t flags) {
    if (flags != 0) { return SYSCALL_FAULT_(EINVAL); }
    fd_t *src_handle = get_fd(get_current_task()->process->fdts, fd_in);
    fd_t *dst_handle = get_fd(get_current_task()->process->fdts, fd_out);
    if (src_handle == NULL || dst_handle == NULL) { return SYSCALL_FAULT_(EBADF); }
    if (dst_handle->offset >= dst_handle->node->size && dst_handle->node->size > 0) return EOK;
    uint64_t src_offset = off_in ? *off_in : src_handle->offset;
    uint64_t dst_offset = off_out ? *off_out : dst_handle->offset;

    uint64_t length     = src_handle->node->size > len ? len : src_handle->node->size;
    uint8_t *buffer     = (uint8_t *)malloc(length);
    size_t   copy_total = 0;
    if (vfs_read(src_handle->node, buffer, src_offset, length) == (size_t)-1) { goto errno_; }
    copy_total = vfs_write(dst_handle->node, buffer, dst_offset, length);
    if (copy_total == (size_t)-1) { goto errno_; }
    vfs_update(dst_handle->node);
    free(buffer);
    dst_handle->offset += copy_total;
    return copy_total;
errno_:
    free(buffer);
    return SYSCALL_FAULT_(EFAULT);
}

syscall_(ftruncate) {
    return EOK;
}

syscall_(rename, char *oldpath, char *newpath) {
    if (!oldpath || !newpath) return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)oldpath, strlen(oldpath))) { return SYSCALL_FAULT_(EFAULT); }
    if (check_user_overflow((uint64_t)newpath, strlen(newpath))) { return SYSCALL_FAULT_(EFAULT); }
    char      *noldpath = vfs_cwd_path_build(oldpath);
    char      *nnewpath = vfs_cwd_path_build(newpath);
    vfs_node_t oldnode  = vfs_open(noldpath);
    if (!oldnode) {
        free(noldpath);
        free(nnewpath);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_node_t newnode = vfs_open(nnewpath);
    if (newnode) { vfs_delete(newnode); }
    size_t     ret        = vfs_rename(oldnode, nnewpath) == EOK ? EOK : SYSCALL_FAULT_(ENOENT);
    char      *parent     = get_parent_path(nnewpath);
    vfs_node_t parent_dir = vfs_open(parent);
    if (!parent_dir) {
        free(parent);
        ret = SYSCALL_FAULT_(ENOENT);
        goto end_rename;
    }
    vfs_close(parent_dir); // 更新父目录的信息
    free(parent);
end_rename:
    free(noldpath);
    free(nnewpath);
    return ret;
}

syscall_(symlink, char *name, char *new) {
    if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_symlink(name, new);
    return ret;
}

syscall_(link, char *name, char *new) {
    if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_link(name, new);

    return ret;
}

syscall_(select, int nfds, uint8_t *read, uint8_t *write, uint8_t *except,
         struct timeval *timeout) {
    if (read && check_user_overflow((uint64_t)read, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (write && check_user_overflow((uint64_t)write, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (except && check_user_overflow((uint64_t)except, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    size_t         complength = sizeof(struct pollfd);
    struct pollfd *comp       = (struct pollfd *)malloc(complength);
    memset(comp, 0, complength);
    size_t compIndex = 0;
    if (read) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(read, i)) select_add(&comp, &compIndex, &complength, i, POLLIN);
        }
    }
    if (write) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(write, i)) select_add(&comp, &compIndex, &complength, i, POLLOUT);
        }
    }
    if (except) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(except, i))
                select_add(&comp, &compIndex, &complength, i, POLLPRI | POLLERR);
        }
    }

    //int toZero = (nfds + 8) / 8;
    int toZero = (nfds + 7) / 8;
    if (read) memset(read, 0, toZero);
    if (write) memset(write, 0, toZero);
    if (except) memset(except, 0, toZero);

    size_t time = 0;
    if (timeout == NULL) {
        time = -1;
    } else if (timeout->tv_sec == -1 || timeout->tv_usec == -1) {
        time = -1;
    } else
        time = (timeout->tv_sec * 1000 + (timeout->tv_usec + 1000) / 1000);

    size_t res = syscall_poll(comp, compIndex, time, 0, 0, 0, 0);

    if ((int64_t)res < 0) {
        free(comp);
        return res;
    }

    size_t verify = 0;
    for (size_t i = 0; i < compIndex; i++) {
        if (!comp[i].revents) continue;
        if (comp[i].events & POLLIN && comp[i].revents & POLLIN) {
            select_bitmap_set(read, comp[i].fd);
            verify++;
        }
        if (comp[i].events & POLLOUT && comp[i].revents & POLLOUT) {
            select_bitmap_set(write, comp[i].fd);
            verify++;
        }
        if ((comp[i].events & POLLPRI && comp[i].revents & POLLPRI)) {
            select_bitmap_set(except, comp[i].fd);
            verify++;
        }
    }

    free(comp);
    return verify;
}

syscall_(pselect6, uint64_t nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
         struct timespec *timeout, WeirdPselect6 *weirdPselect6) {
    if (readfds && check_user_overflow((uint64_t)readfds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (writefds && check_user_overflow((uint64_t)writefds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (exceptfds && check_user_overflow((uint64_t)exceptfds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    size_t    sigsetsize = weirdPselect6->ss_len;
    sigset_t *sigmask    = weirdPselect6->ss;
    if (sigsetsize < sizeof(sigset_t)) { return SYSCALL_FAULT_(EINVAL); }
    sigset_t origmask = 0;
    if (sigmask) { syscall_ssetmask(SIG_SETMASK, sigmask, &origmask, 0, 0, 0, regs); }
    struct timeval timeoutConv;
    if (timeout) {
        timeoutConv = (struct timeval){.tv_sec  = (long)timeout->tv_sec,
                                       .tv_usec = (long)(timeout->tv_nsec + 1000l) / 1000};
    } else {
        timeoutConv = (struct timeval){.tv_sec = (long)-1, .tv_usec = (long)-1};
    }

    size_t ret = syscall_select((uint64_t)nfds, (uint8_t *)readfds, (uint8_t *)writefds,
                                (uint8_t *)exceptfds, &timeoutConv, 0, 0);
    if (sigmask) { syscall_ssetmask(SIG_SETMASK, &origmask, NULL, 0, 0, 0, regs); }
    return ret;
}

syscall_(getdents, int fd, struct dirent *dents, size_t size) {
    if (unlikely(check_user_overflow((uint64_t)dents, size))) { return SYSCALL_FAULT_(EFAULT); }
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL)) { return SYSCALL_FAULT_(EBADF); }
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }
    size_t   child_count   = (uint64_t)list_length(handle->node->child);
    size_t   max_dents_num = size / sizeof(struct dirent);
    size_t   read_count    = 0;
    uint64_t offset        = 0;
    list_foreach(handle->node->child, i) {
        if (offset < handle->offset) { goto next; }
        if (handle->offset >= (child_count * sizeof(struct dirent))) { break; }
        if (read_count >= max_dents_num) { break; }
        vfs_node_t child_node      = (vfs_node_t)i->data;
        dents[read_count].d_ino    = (long)child_node->inode;
        dents[read_count].d_off    = (long)handle->offset;
        dents[read_count].d_reclen = sizeof(struct dirent);
        if (child_node->type & file_symlink) {
            dents[read_count].d_type = DT_LNK;
        } else if (child_node->type & file_none) {
            dents[read_count].d_type = DT_REG;
        } else if (child_node->type & file_block) {
            dents[read_count].d_type = DT_BLK;
        } else if (child_node->type & file_stream) {
            dents[read_count].d_type = DT_CHR;
        } else if (child_node->type & file_socket) {
            dents[read_count].d_type = DT_SOCK;
        } else if (child_node->type & file_dir) {
            dents[read_count].d_type = DT_DIR;
        } else {
            dents[read_count].d_type = DT_UNKNOWN;
        }
        strncpy(dents[read_count].d_name, child_node->name, 256);
        handle->offset += sizeof(struct dirent);
        read_count++;
    next:
        offset += sizeof(struct dirent);
    }
    return read_count * sizeof(struct dirent);
}

syscall_(newfstatat, int dirfd, char *pathname, struct stat *buf, uint64_t flags) {
    char    *resolved = at_resolve_pathname(dirfd, pathname);
    uint64_t ret      = syscall_stat(resolved, buf, 0, 0, 0, 0, regs);
    free(resolved);
    return ret;
}

syscall_(statx, int dirfd, char *pathname, uint64_t flags, uint64_t mask, struct statx *buff) {
    if (unlikely(!pathname || check_user_overflow((uint64_t)pathname, strlen(pathname)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (unlikely(!buff || check_user_overflow((uint64_t)buff, sizeof(struct statx)))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    struct stat simple;
    memset(&simple, 0, sizeof(struct stat));
    uint64_t ret = syscall_newfstatat(dirfd, pathname, &simple, flags, 0, 0, 0);
    if ((int64_t)ret < 0) return ret;

    buff->stx_mask            = mask;
    buff->stx_blksize         = simple.st_blksize;
    buff->stx_attributes      = 0;
    buff->stx_nlink           = simple.st_nlink;
    buff->stx_uid             = simple.st_uid;
    buff->stx_gid             = simple.st_gid;
    buff->stx_mode            = simple.st_mode;
    buff->stx_ino             = simple.st_ino;
    buff->stx_size            = simple.st_size;
    buff->stx_blocks          = simple.st_blocks;
    buff->stx_attributes_mask = 0;

    buff->stx_atime.tv_sec  = (long)simple.st_atim.tv_sec;
    buff->stx_atime.tv_nsec = simple.st_atim.tv_nsec;

    buff->stx_btime.tv_sec  = (long)simple.st_ctim.tv_sec;
    buff->stx_btime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_ctime.tv_sec  = (long)simple.st_ctim.tv_sec;
    buff->stx_ctime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_mtime.tv_sec  = (long)simple.st_mtim.tv_sec;
    buff->stx_mtime.tv_nsec = simple.st_mtim.tv_nsec;
    return EOK;
}

syscall_(pipe2, int *pipefd, uint64_t flags) {
    /* fs/pipefs.c */
    extern vfs_node_t pipefs_root;
    extern int        pipefd_id;
    extern int        pipefs_id;

    if (pipefs_root == NULL) return SYSCALL_FAULT_(ENOSYS);

    char buf[16];
    sprintf(buf, "pipe%d", pipefd_id++);

    vfs_node_t node_input = vfs_node_alloc(pipefs_root, buf);
    node_input->type      = file_pipe;
    node_input->fsid      = pipefs_id;
    node_input->refcount++;
    pipefs_root->mode = 0700;

    sprintf(buf, "pipe%d", pipefd_id++);
    vfs_node_t node_output = vfs_node_alloc(pipefs_root, buf);
    node_output->type      = file_pipe;
    node_output->fsid      = pipefs_id;
    node_output->refcount++;
    pipefs_root->mode = 0700;

    pipe_info_t *info = (pipe_info_t *)malloc(sizeof(pipe_info_t));
    memset(info, 0, sizeof(pipe_info_t));
    info->buf       = calloc(1, PIPE_BUFF);
    info->read_fds  = 1;
    info->write_fds = 1;
    info->ptr       = 0;
    info->lock      = SPIN_INIT;

    pipe_specific_t *read_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    read_spec->write           = false;
    read_spec->info            = info;
    read_spec->node            = node_input;

    pipe_specific_t *write_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    write_spec->write           = true;
    write_spec->info            = info;
    write_spec->node            = node_output;

    node_input->handle  = read_spec;
    node_output->handle = write_spec;

    fdt_t *fd_table   = get_current_task()->process->fdts;
    fd_t  *handle_in  = malloc(sizeof(fd_t));
    handle_in->node   = node_input;
    handle_in->offset = 0;
    handle_in->flags  = flags;
    handle_in->fd     = add_fd(fd_table, handle_in);

    fd_t *handle_out   = malloc(sizeof(fd_t));
    handle_out->node   = node_output;
    handle_out->offset = 0;
    handle_out->flags  = flags;
    handle_out->fd     = add_fd(fd_table, handle_out);

    pipefd[0] = (int)handle_in->fd;
    pipefd[1] = (int)handle_out->fd;

    return EOK;
}

syscall_(pipe, int *pipefd) {
    return syscall_pipe2(pipefd, 0, 0, 0, 0, 0, regs);
}

syscall_(unlink, char *name) {
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char      *npath = vfs_cwd_path_build(name);
    vfs_node_t node  = vfs_open(npath);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_none && node->type != file_symlink) {
        vfs_close(node);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 0) {
        node->refcount--;
        return EOK;
    } else
        ret = vfs_delete(node) == EOK ? EOK : SYSCALL_FAULT_(ENOENT);
    free(npath);
    return ret;
}

syscall_(rmdir, char *name) {
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char      *n_name = vfs_cwd_path_build(name);
    vfs_node_t node   = vfs_open(n_name);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_dir) {
        vfs_close(node);
        free(n_name);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 0) {
        node->refcount--;
        ret = EOK;
    } else {
        ret = vfs_delete(node);
    }
    free(n_name);
    return ret;
}

syscall_(unlinkat, int dirfd, char *name) {
    if (check_user_overflow((uint64_t)name, strlen(name))) { return (uint64_t)-EFAULT; }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return -ENOENT;

    uint64_t ret = syscall_unlink(path, 0, 0, 0, 0, 0, regs);

    free(path);

    return ret;
}

syscall_(access, char *filename) {
    struct stat buf;
    return syscall_stat(filename, &buf, 0, 0, 0, 0, regs);
}

syscall_(mkdir, char *name, uint64_t mode) {
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char  *npath = vfs_cwd_path_build(name);
    size_t ret   = vfs_mkdir(npath) == EOK ? EOK : -1;
    free(npath);
    return ret;
}
