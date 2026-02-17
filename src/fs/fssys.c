#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/devtmpfs.h"
#include "fs/fds.h"
#include "fs/pipefs.h"
#include "fs/sockfs.h"
#include "fs/vfs.h"
#include "mem/frame.h"
#include "syscall.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

static void free_private_open_node(vfs_node_t node) {
    if (node == NULL)
        return;
    node->child = list_free(node->child);
    if (node->name)
        free(node->name);
    if (node->linkto_path)
        free(node->linkto_path);
    free(node);
}

static vfs_node_t devtmpfs_make_per_open_node(vfs_node_t node) {
    if (node == NULL || node->fsid != dev_tmpfs_id)
        return node;

    dtmp_handle_t *template = (dtmp_handle_t *)node->handle;
    if (template == NULL || template->type != dtp_file_device || template->open_t == NULL) {
        return node;
    }

    vfs_node_t private_node = vfs_node_alloc(node->parent, node->name);
    if (private_node == NULL)
        return NULL;
    if (node->parent) {
        list_delete(node->parent->child, private_node);
    }

    private_node->root        = node->root;
    private_node->fsid        = node->fsid;
    private_node->type        = file_none;
    private_node->size        = node->size;
    private_node->realsize    = node->realsize;
    private_node->owner       = node->owner;
    private_node->group       = node->group;
    private_node->permissions = node->permissions;
    private_node->mode        = node->mode;
    private_node->dev         = node->dev;
    private_node->rdev        = node->rdev;
    private_node->flags       = node->flags | VFS_NODE_FLAG_PRIVATE_FD;

    dtmp_handle_t *private_handle = calloc(1, sizeof(dtmp_handle_t));
    if (private_handle == NULL) {
        free_private_open_node(private_node);
        return NULL;
    }
    memcpy(private_handle, template, sizeof(dtmp_handle_t));
    private_handle->node          = private_node;
    private_handle->device_handle = NULL;
    private_handle->is_per_open   = true;
    private_node->handle          = private_handle;

    private_handle->open_t(
        private_node->parent ? private_node->parent->handle : NULL, private_node->name, private_node
    );

    if (!(private_node->type & (file_ptmx | file_pts))) {
        private_node->type = private_handle->dev_type == device_stream ? file_stream : file_block;
    }
    if (private_handle->size_t) {
        private_node->size = private_handle->size_t(private_handle->device_handle);
    }

    if (private_handle->device_handle == NULL) {
        free(private_handle);
        private_node->handle = NULL;
        free_private_open_node(private_node);
        return NULL;
    }

    return private_node;
}

syscall_(open, char *path0, uint64_t flags, uint64_t mode) {
    if (unlikely(path0 == NULL))
        return SYSCALL_FAULT_(EINVAL);
    pcb_t process = get_current_task()->process;

    char *normalized_path = vfs_cwd_path_build(path0);

    // /dev/tty should resolve to the process's controlling terminal
    if (strcmp(normalized_path, "/dev/tty") == 0) {
        pcb_t proc = process;
        if (proc->ctty_path) {
            free(normalized_path);
            normalized_path = strdup(proc->ctty_path);
        }
    }

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
            else {
                if (flags & O_CREAT) {
                    uint16_t final_mode = (uint16_t)(mode & 0777);
                    final_mode &= (uint16_t)~(process->umask & 0777);
                    vfs_chmod(node, final_mode);
                }
                goto next;
            }
        } else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

next:;
    vfs_node_t private_node = devtmpfs_make_per_open_node(node);
    if (private_node == NULL) {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOMEM);
    }
    node = private_node;

    fd_t *fd_handle = calloc(1, sizeof(fd_t));
    not_null_assert(fd_handle, "sys_open: null alloc fd");
    fd_handle->offset = flags & O_APPEND ? node->size : 0;
    fd_handle->node   = node;
    fd_handle->flags  = flags;
    int index         = add_fd(process->fdts, fd_handle);
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
    if (unlikely(fd < 0))
        return SYSCALL_FAULT_(EINVAL);
    fdt_t *fdt    = get_current_task()->process->fdts;
    fd_t  *handle = (fd_t *)get_fd(fdt, fd);
    if (handle == NULL)
        return SYSCALL_FAULT_(EBADF);
    vfs_close(handle->node);
    remove_fd(fdt, fd);
    return EOK;
}

static volatile bool is_debug;
void                 debug_read(int fd) {
    char buffer[512];
    syscall_read(fd, (uint8_t *)buffer, 512, 0, 0, 0, NULL);
    if (fd)
        buffer[0] = 0;
}

syscall_(write, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL))
        return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0))
        return EOK;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (!handle)
        return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_pipe) {
        size_t ret = vfs_write(handle->node, buffer, 0, size);
        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EIO);
        return ret;
    }
    if (handle->node->type & file_socket) {
        size_t ret = vfs_write(handle->node, buffer, 0, size);
        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EPIPE);
        return ret;
    }
    // Streaming devices (terminals, PTY, eventfd) don't use file offsets
    if (handle->node->type & (file_stream | file_ptmx | file_pts | file_eventfd)) {
        size_t ret = vfs_write(handle->node, buffer, 0, size);
        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EIO);
        return ret;
    }
    size_t ret = vfs_write(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)-1)
        return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1)
        handle->offset += ret;
    vfs_update(handle->node);

    if (is_debug)
        debug_read(fd);
    return ret;
}

syscall_(read, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL))
        return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0))
        return EOK;
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (!handle)
        return SYSCALL_FAULT_(EBADF);

    if (handle->node->type & file_pipe) {
        // logkf("[fd-dbg] pid=%d read(%d, size=%d) [pipe]\n",
        // get_current_task()->process->pid, fd, size);

        vfs_update(handle->node);
        if (handle->node->size == 0 && handle->flags & O_NONBLOCK) {
            return SYSCALL_FAULT_(EWOULDBLOCK);
        }
        size_t ret = vfs_read(handle->node, buffer, 0, size);

        // logkf("[fd-dbg] pid=%d read(%d) = %d\n",
        //       get_current_task()->process->pid, fd, (int)ret);

        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EIO);
        return ret;
    }
    if (handle->node->type & file_socket) {
        size_t ret = vfs_read(handle->node, buffer, 0, size);
        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EPIPE);
        return ret;
    }
    // Streaming devices (terminals, PTY, eventfd) don't use file offsets
    if (handle->node->type & (file_stream | file_ptmx | file_pts | file_eventfd)) {
        size_t ret = vfs_read(handle->node, buffer, 0, size);
        if (ret == (size_t)-1)
            return SYSCALL_FAULT_(EIO);
        return ret;
    }
    if (handle->node->size != (uint64_t)-1) {
        if (handle->offset >= handle->node->size) {
            return EOK;
        }
    }
    size_t ret = vfs_read(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)-1)
        return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) {
        handle->offset += ret;
    }
    return ret;
}

syscall_(writev, int fd, struct iovec *iov, int iovcnt) {
    if (unlikely(fd < 0 || iov == NULL))
        return SYSCALL_FAULT_(EINVAL);
    if (iovcnt == 0)
        return EOK;
    fd_t  *handle = get_fd(get_current_task()->process->fdts, fd);
    size_t total  = 0;
    for (int i = 0; i < iovcnt; i++) {
        size_t status = vfs_write(handle->node, iov[i].iov_base, handle->offset, iov[i].iov_len);
        if (status == (size_t)-1)
            return total;
        if (!(handle->node->type & file_pipe) && handle->node->size != (uint64_t)-1) {
            handle->offset += status;
        }
        total += iov[i].iov_len;
    }
    return total;
}

syscall_(readv, int fd, struct iovec *iov, int iovcnt0) {
    if (unlikely(fd < 0 || iov == NULL))
        return SYSCALL_FAULT_(EINVAL);
    if (iovcnt0 == 0)
        return EOK;
    size_t iovcnt = iovcnt0;
    fd_t  *handle = get_fd(get_current_task()->process->fdts, fd);
    if (iovcnt == 0)
        return 0;
    if (handle == NULL)
        return SYSCALL_FAULT_(EBADF);
    size_t buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf = (uint8_t *)malloc(buf_len);
    if (!(handle->node->type & file_pipe) && handle->node->size != (uint64_t)-1) {
        if (handle->offset > handle->node->size) {
            free(buf);
            return EOK;
        }
    }
    size_t status = vfs_read(handle->node, buf, handle->offset, buf_len);
    if (status == (size_t)-1) {
        free(buf);
        return SYSCALL_FAULT_(EIO);
    }
    if (!(handle->node->type & file_pipe) && handle->node->size != (uint64_t)-1) {
        handle->offset += status;
    }
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        size_t len = iov[i].iov_len;
        if (len == 0)
            continue;

        size_t to_copy = len;
        if (copied + to_copy > status) {
            to_copy = status - copied;
        }

        memcpy(iov[i].iov_base, buf + copied, to_copy);
        copied += to_copy;
    }
    free(buf);
    return status;
}

static inline void vfs_fill_stat(vfs_node_t node, struct stat *buf) {
    buf->st_gid  = (int)node->group;
    buf->st_uid  = (int)node->owner;
    buf->st_ino  = node->inode;
    buf->st_size = (long long int)node->size;
    buf->st_mode = node->mode
                   | (node->type == file_symlink  ? S_IFLNK
                      : node->type == file_dir    ? S_IFDIR
                      : node->type == file_block  ? S_IFBLK
                      : node->type == file_socket ? S_IFSOCK
                      : node->type == file_none   ? S_IFREG
                      : node->type == file_stream ? S_IFCHR
                      : node->type == file_ptmx   ? S_IFCHR
                      : node->type == file_pts    ? S_IFCHR
                                                  : S_IFREG);
    buf->st_nlink = 1;
    buf->st_dev   = (long)node->dev;
    buf->st_rdev  = (long)node->rdev;
}

syscall_(stat, char *fn, struct stat *buf) {
    if (unlikely(fn == NULL || buf == NULL))
        return SYSCALL_FAULT_(EINVAL);
    char      *path = vfs_cwd_path_build(fn);
    vfs_node_t node = vfs_open(path);

    if (node == NULL) {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_fill_stat(node, buf);
    free(path);
    return EOK;
}

syscall_(lstat, char *fn, struct stat *buf) {
    if (unlikely(fn == NULL || buf == NULL))
        return SYSCALL_FAULT_(EINVAL);
    char      *path = vfs_cwd_path_build(fn);
    vfs_node_t node = vfs_open_nofollow(path);
    free(path);
    if (node == NULL) {
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_fill_stat(node, buf);
    vfs_close(node);
    return EOK;
}

syscall_(ioctl, int fd, size_t options, void *arg2) {
    if (unlikely(fd < 0))
        return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (handle == NULL)
        return SYSCALL_FAULT_(EBADF);
    errno_t ret = vfs_ioctl(handle->node, options, arg2);
    return ret;
}

static int ensure_fdt_capacity(fdt_t *fdt, int expect_fd) {
    while ((size_t)expect_fd >= fdt->fds_length) {
        size_t old_len = fdt->fds_length;
        size_t new_len = old_len * FD_GROWTH_FACTOR;
        if (new_len == 0)
            new_len = FD_INITIAL_CAPACITY;
        if (new_len <= (size_t)expect_fd)
            new_len = (size_t)expect_fd + 1;
        fd_t **new_fds = (fd_t **)realloc(fdt->fds, new_len * sizeof(fd_t *));
        if (!new_fds)
            return -ENOMEM;
        memset(&new_fds[old_len], 0, (new_len - old_len) * sizeof(fd_t *));
        fdt->fds        = new_fds;
        fdt->fds_length = new_len;
    }
    return EOK;
}

static uint64_t dup_with_minfd(fd_t *handle, int min_fd, bool cloexec) {
    if (unlikely(min_fd < 0))
        return SYSCALL_FAULT_(EINVAL);
    fdt_t *fdt = get_current_task()->process->fdts;
    if (ensure_fdt_capacity(fdt, min_fd) < 0)
        return SYSCALL_FAULT_(ENOMEM);

    int newfd = min_fd;
    while (true) {
        if ((size_t)newfd >= fdt->fds_length) {
            if (ensure_fdt_capacity(fdt, newfd) < 0)
                return SYSCALL_FAULT_(ENOMEM);
        }
        if (fdt->fds[newfd] == NULL)
            break;
        newfd++;
    }

    fd_t *new_handle = fd_dup(handle);
    if (new_handle == NULL)
        return SYSCALL_FAULT_(ENOMEM);
    new_handle->fd = newfd;
    new_handle->flags &= ~O_CLOEXEC;
    if (cloexec)
        new_handle->flags |= O_CLOEXEC;
    fdt->fds[newfd] = new_handle;
    return newfd;
}

syscall_(dup2, int fd, int newfd) {
    if (unlikely(newfd < 0))
        return SYSCALL_FAULT_(EINVAL);
    fdt_t *fdt    = get_current_task()->process->fdts;
    fd_t  *handle = get_fd(fdt, fd);
    if (unlikely(handle == NULL))
        return SYSCALL_FAULT_(EBADF);
    if (fd == newfd)
        return newfd;

    if (ensure_fdt_capacity(fdt, newfd) < 0)
        return SYSCALL_FAULT_(ENOMEM);

    fd_t *old_handle = fdt->fds[newfd];
    if (old_handle != NULL) {
        vfs_close(old_handle->node);
        fdt->fds[newfd] = NULL;
        free(old_handle);
    }

    fd_t *new_handle = fd_dup(handle);
    if (new_handle == NULL)
        return SYSCALL_FAULT_(ENOMEM);
    new_handle->fd = newfd;
    new_handle->flags &= ~O_CLOEXEC; // POSIX: dup2 clears close-on-exec
    fdt->fds[newfd] = new_handle;
    return newfd;
}

syscall_(dup3, int oldfd, int newfd, int flags) {
    if (flags & ~O_CLOEXEC)
        return SYSCALL_FAULT_(EINVAL);
    if (oldfd == newfd)
        return SYSCALL_FAULT_(EINVAL);
    uint64_t ret = syscall_dup2(oldfd, newfd, 0, 0, 0, 0, regs);
    if ((int64_t)ret < 0)
        return ret;
    if (flags & O_CLOEXEC) {
        fd_t *h = get_fd(get_current_task()->process->fdts, newfd);
        if (h)
            h->flags |= O_CLOEXEC;
    }
    return ret;
}

syscall_(dup, int fd) {
    if (unlikely(fd < 0))
        return SYSCALL_FAULT_(EINVAL);
    fdt_t *fdt    = get_current_task()->process->fdts;
    fd_t  *handle = get_fd(fdt, fd);
    if (handle == NULL)
        return SYSCALL_FAULT_(EBADF);
    fd_t *new_handle = fd_dup(handle);
    new_handle->flags &= ~O_CLOEXEC; // POSIX: dup clears close-on-exec
    return new_handle->fd = add_fd(fdt, new_handle);
}

syscall_(getcwd, char *buffer, size_t length) {
    if (unlikely(buffer == NULL))
        return SYSCALL_FAULT_(EINVAL);
    if (unlikely(length == 0))
        return EOK;
    pcb_t  process  = get_current_task()->process;
    char  *cwd      = vfs_get_fullpath(process->cwd);
    size_t cwd_leng = strlen(cwd);
    if (length > cwd_leng)
        length = cwd_leng;
    memcpy(buffer, cwd, length);
    free(cwd);
    return length;
}

syscall_(chdir, char *s) {
    if (unlikely(s == NULL))
        return SYSCALL_FAULT_(EINVAL);
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

    if (unlikely(normalized_path == NULL)) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL) {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (node->type == file_dir) {
        process->cwd = node;
    } else {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOTDIR);
    }

    free(normalized_path);
    return EOK;
}

syscall_(fcntl, int fd, int cmd, uint64_t arg) {
    if (fd < 0 || cmd < 0)
        return SYSCALL_FAULT_(EINVAL);
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (handle == NULL)
        return SYSCALL_FAULT_(EBADF);

    switch (cmd) {
    case F_GETFD:
        return (handle->flags & O_CLOEXEC) ? 1 : 0;
    case F_SETFD:
        if (arg & 1)
            handle->flags |= O_CLOEXEC;
        else
            handle->flags &= ~O_CLOEXEC;
        return EOK;
    case F_DUPFD_CLOEXEC:
        return dup_with_minfd(handle, (int)arg, true);
    case F_DUPFD:
        return dup_with_minfd(handle, (int)arg, false);
    case F_GETFL:
        return handle->flags;
    case F_SETFL:;
        uint32_t valid_flags = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        handle->flags &= ~valid_flags;
        handle->flags |= arg & valid_flags;
        handle->node->flags &= ~valid_flags;
        handle->node->flags |= arg & valid_flags;
        return EOK;
    default:
        break;
    }
    return EOK;
}

syscall_(mount, char *dev_name, char *dir_name, char *type, uint64_t flags, void *data) {
    if (dir_name == NULL)
        return SYSCALL_FAULT_(EINVAL);

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
        if (old_root == NULL || !old_root->is_mount)
            return SYSCALL_FAULT_(EINVAL);
        if (dir != rootdir)
            list_append(dir->parent->child, old_root);
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

    if (type == NULL)
        return SYSCALL_FAULT_(EINVAL);

    char   *ndev_name = vfs_cwd_path_build(dev_name);
    errno_t mret      = EOK;
mount:
    mret = vfs_mount((const char *)ndev_name, type, dir);
    if (mret != EOK) {
        free(ndir_name);
        free(ndev_name);
        if (mret < 0)
            return (uint64_t)mret;
        return SYSCALL_FAULT_(EIO);
    }
    free(ndir_name);
    free(ndev_name);
    return EOK;
}

syscall_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout) {
    int      ready      = 0;
    uint64_t start_time = nano_time();
    bool     sigexit    = false;
    tcb_t    current    = get_current_task();
    fdt_t   *fdt        = current->process->fdts;

    extern vfs_callback_t fs_callbacks[256];

    do {
        ready = 0;
        // 清零所有 revents
        for (size_t i = 0; i < nfds; i++) {
            fds_user[i].revents = 0;
        }

        // 检查每个文件描述符
        for (size_t i = 0; i < nfds; i++) {
            fd_t *handle = get_fd(fdt, fds_user[i].fd);
            if (handle == NULL) {
                fds_user[i].revents = POLLNVAL;
                ready++;
                continue;
            }
            vfs_node_t node = handle->node;
            if (fs_callbacks[node->fsid]->poll == (void *)dummy) {
                if (fds_user[i].events & POLLIN || fds_user[i].events & POLLOUT) {
                    fds_user[i].revents = fds_user[i].events & POLLIN ? POLLIN : POLLOUT;
                    ready++;
                }
                continue;
            }
            int revents =
                (int)epoll_to_poll_comp(vfs_poll(node, poll_to_epoll_comp(fds_user[i].events)));
            if (revents > 0) {
                fds_user[i].revents = (short)revents;
                ready++;
            }
        }

        sigexit = signals_pending_quick(current);

        if (ready > 0 || sigexit)
            break;

        scheduler_yield();
    } while (timeout != 0
             && ((int)timeout == -1 || (nano_time() - start_time) < timeout * 1000000ULL));

    if (!ready && sigexit)
        return (size_t)-EINTR;
    return ready;
}

syscall_(fstat, int fd, struct stat *buf) {
    if (unlikely(buf == NULL))
        return SYSCALL_FAULT_(EINVAL);

    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL))
        return SYSCALL_FAULT_(EBADF);
    vfs_node_t node = handle->node;
    buf->st_gid     = (int)node->group;
    buf->st_uid     = (int)node->owner;
    buf->st_size    = node->size == (uint64_t)-1 ? 0 : (long long int)node->size;
    buf->st_mode    = node->type
                   | (node->type == file_symlink  ? S_IFLNK
                      : node->type == file_dir    ? S_IFDIR
                      : node->type == file_block  ? S_IFBLK
                      : node->type == file_socket ? S_IFSOCK
                      : node->type == file_none   ? S_IFREG
                      : node->type == file_stream ? S_IFCHR
                      : node->type == file_ptmx   ? S_IFCHR
                      : node->type == file_pts    ? S_IFCHR
                                                  : 0);
    buf->st_nlink = 1;
    buf->st_dev   = node->dev;
    buf->st_rdev  = node->rdev;
    buf->st_ctim = buf->st_atim = buf->st_ctim = buf->st_mtim =
        (struct timespec){ .tv_sec  = node->createtime / 1000000000ULL,
                           .tv_nsec = node->createtime % 1000000000ULL };
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
    if (status != EOK)
        ret = SYSCALL_FAULT_(EBUSY);
    free(path);
    return ret;
}

syscall_(lseek, int fd, size_t offset, size_t whence) {
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL))
        return SYSCALL_FAULT_(EBADF);

    int64_t real_offset = (int64_t)offset;
    if (real_offset < 0 && handle->node->type & file_none && whence != SEEK_CUR)
        return SYSCALL_FAULT_(EBADF);
    switch (whence) {
    case SEEK_SET:
        handle->offset = real_offset;
        if ((handle->node->type & file_dir) && real_offset == 0) {
            handle->dir_last = NULL;
        }
        break;
    case SEEK_CUR:
        handle->offset += real_offset;
        if ((int64_t)handle->offset < 0) {
            handle->offset = 0;
        } else if (handle->offset > handle->node->size) {
            handle->offset = handle->node->size;
        }
        break;
    case SEEK_END:
        handle->offset = handle->node->size - real_offset;
        break;
    case SEEK_DATA:
        if (offset >= handle->node->size)
            return SYSCALL_FAULT_(ENXIO);
        break;
    case SEEK_HOLE:
        if (offset >= handle->node->size)
            return SYSCALL_FAULT_(ENXIO);
        return handle->node->size;
    default:
        return SYSCALL_FAULT_(ENXIO);
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

syscall_(
    copy_file_range, int fd_in, uint64_t *off_in, int fd_out, uint64_t *off_out, size_t len,
    uint64_t flags
) {
    if (flags != 0) {
        return SYSCALL_FAULT_(EINVAL);
    }
    fdt_t *fdt        = get_current_task()->process->fdts;
    fd_t  *src_handle = get_fd(fdt, fd_in);
    fd_t  *dst_handle = get_fd(fdt, fd_out);
    if (src_handle == NULL || dst_handle == NULL) {
        return SYSCALL_FAULT_(EBADF);
    }
    if (dst_handle->offset >= dst_handle->node->size && dst_handle->node->size > 0)
        return EOK;
    uint64_t src_offset = off_in ? *off_in : src_handle->offset;
    uint64_t dst_offset = off_out ? *off_out : dst_handle->offset;

    uint64_t length     = src_handle->node->size > len ? len : src_handle->node->size;
    uint8_t *buffer     = (uint8_t *)malloc(length);
    size_t   copy_total = 0;
    if (vfs_read(src_handle->node, buffer, src_offset, length) == (size_t)-1) {
        goto errno_;
    }
    copy_total = vfs_write(dst_handle->node, buffer, dst_offset, length);
    if (copy_total == (size_t)-1) {
        goto errno_;
    }
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
    if (!oldpath || !newpath)
        return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)oldpath, strlen(oldpath))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (check_user_overflow((uint64_t)newpath, strlen(newpath))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char      *noldpath = vfs_cwd_path_build(oldpath);
    char      *nnewpath = vfs_cwd_path_build(newpath);
    vfs_node_t oldnode  = vfs_open(noldpath);
    if (!oldnode) {
        free(noldpath);
        free(nnewpath);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_node_t newnode = vfs_open(nnewpath);
    if (newnode) {
        vfs_delete(newnode);
    }
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
    if (name == NULL || new == NULL)
        return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)name, strlen(name))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (check_user_overflow((uint64_t)new, strlen(new))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char   *linkpath = vfs_cwd_path_build(new);
    errno_t ret      = vfs_symlink(linkpath, name);
    free(linkpath);
    return ret < 0 ? SYSCALL_FAULT_(-ret) : ret;
}

syscall_(link, char *name, char *new) {
    if (name == NULL || new == NULL)
        return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)name, strlen(name))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (check_user_overflow((uint64_t)new, strlen(new))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char   *linkpath = vfs_cwd_path_build(new);
    errno_t ret      = vfs_link(linkpath, name);
    free(linkpath);
    return ret < 0 ? SYSCALL_FAULT_(-ret) : ret;
}

syscall_(
    select, int nfds, uint8_t *read, uint8_t *write, uint8_t *except, struct timeval *timeout
) {
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
            if (select_bitmap(read, i))
                select_add(&comp, &compIndex, &complength, i, POLLIN);
        }
    }
    if (write) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(write, i))
                select_add(&comp, &compIndex, &complength, i, POLLOUT);
        }
    }
    if (except) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(except, i))
                select_add(&comp, &compIndex, &complength, i, POLLPRI | POLLERR);
        }
    }

    // int toZero = (nfds + 8) / 8;
    int toZero = (nfds + 7) / 8;
    if (read)
        memset(read, 0, toZero);
    if (write)
        memset(write, 0, toZero);
    if (except)
        memset(except, 0, toZero);

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
        if (!comp[i].revents)
            continue;
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

syscall_(
    pselect6, uint64_t nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    struct timespec *timeout, WeirdPselect6 *weirdPselect6
) {
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
    if (sigsetsize < sizeof(sigset_t)) {
        return SYSCALL_FAULT_(EINVAL);
    }
    sigset_t origmask = 0;
    if (sigmask) {
        syscall_ssetmask(SIG_SETMASK, sigmask, &origmask, 0, 0, 0, regs);
    }
    struct timeval timeoutConv;
    if (timeout) {
        timeoutConv = (struct timeval){ .tv_sec  = (long)timeout->tv_sec,
                                        .tv_usec = (long)(timeout->tv_nsec + 1000l) / 1000 };
    } else {
        timeoutConv = (struct timeval){ .tv_sec = (long)-1, .tv_usec = (long)-1 };
    }

    size_t ret = syscall_select(
        (uint64_t)nfds, (uint8_t *)readfds, (uint8_t *)writefds, (uint8_t *)exceptfds, &timeoutConv,
        0, 0
    );
    if (sigmask) {
        syscall_ssetmask(SIG_SETMASK, &origmask, NULL, 0, 0, 0, regs);
    }
    return ret;
}

USED static void debug_handle(fd_t *handle) {
    list_foreach(handle->node->child, i) {
        logkf("%p\r\n", i->data);
    }
    logkf("\r\n");
}

syscall_(getdents, int fd, struct dirent *dents, size_t size) {
    if (unlikely(check_user_overflow((uint64_t)dents, size))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (unlikely(handle == NULL)) {
        return SYSCALL_FAULT_(EBADF);
    }
    if (handle->node->type != file_dir) {
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t max_dents_num = size / sizeof(struct dirent);
    size_t read_count    = 0;
    if (max_dents_num == 0) {
        return 0;
    }

    list_t start = handle->node->child;
    if (handle->dir_last) {
        list_t cur = handle->node->child;
        while (cur && cur->data != handle->dir_last) {
            cur = cur->next;
        }
        if (cur) {
            start = cur->next;
        } else {
            handle->dir_last = NULL;
        }
    }

    for (list_t it = start; it; it = it->next) {
        if (read_count >= max_dents_num) {
            break;
        }
        vfs_node_t child_node = (vfs_node_t)it->data;
        handle->dir_last      = child_node;
        if (child_node->type & file_delete) {
            continue;
        }
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
    }
    return read_count * sizeof(struct dirent);
}

syscall_(newfstatat, int dirfd, char *pathname, struct stat *buf, uint64_t flags) {
    char *resolved = at_resolve_pathname(dirfd, pathname);
    if (resolved == NULL)
        return SYSCALL_FAULT_(ENOENT);
    uint64_t ret;
    if (flags & AT_SYMLINK_NOFOLLOW) {
        ret = syscall_lstat(resolved, buf, 0, 0, 0, 0, regs);
    } else {
        ret = syscall_stat(resolved, buf, 0, 0, 0, 0, regs);
    }
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
    if ((int64_t)ret < 0)
        return ret;

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

    if (pipefs_root == NULL)
        return SYSCALL_FAULT_(ENOSYS);

    char buf[16];
    sprintf(buf, "pipe%d", pipefd_id++);

    vfs_node_t node_input = vfs_node_alloc(pipefs_root, buf);
    node_input->type      = file_pipe;
    node_input->fsid      = pipefs_id;
    pipefs_root->mode     = 0700;

    sprintf(buf, "pipe%d", pipefd_id++);
    vfs_node_t node_output = vfs_node_alloc(pipefs_root, buf);
    node_output->type      = file_pipe;
    node_output->fsid      = pipefs_id;
    pipefs_root->mode      = 0700;

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
    read_spec->active          = 0;
    read_spec->free_pending    = false;

    pipe_specific_t *write_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    write_spec->write           = true;
    write_spec->info            = info;
    write_spec->node            = node_output;
    write_spec->active          = 0;
    write_spec->free_pending    = false;

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
    if (name == NULL)
        return SYSCALL_FAULT_(EINVAL);
    char      *npath = vfs_cwd_path_build(name);
    vfs_node_t node  = vfs_open_nofollow(npath);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_none && node->type != file_symlink) {
        vfs_close(node);
        return SYSCALL_FAULT_(ENOTDIR);
    }

    size_t ret = vfs_delete(node) == EOK ? EOK : SYSCALL_FAULT_(ENOENT);
    if (ret == EOK) {
        errno_t c = vfs_close(node);
        if (c < 0)
            ret = SYSCALL_FAULT_(-c);
    } else {
        vfs_close(node);
    }
    free(npath);
    return ret;
}

syscall_(rmdir, char *name) {
    if (name == NULL)
        return SYSCALL_FAULT_(EINVAL);
    char      *n_name = vfs_cwd_path_build(name);
    vfs_node_t node   = vfs_open_nofollow(n_name);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_dir) {
        vfs_close(node);
        free(n_name);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    if (node->child != NULL) {
        vfs_close(node);
        free(n_name);
        return SYSCALL_FAULT_(ENOTEMPTY);
    }
    size_t ret = vfs_delete(node) == EOK ? EOK : SYSCALL_FAULT_(ENOENT);
    if (ret == EOK) {
        errno_t c = vfs_close(node);
        if (c < 0)
            ret = SYSCALL_FAULT_(-c);
    } else {
        vfs_close(node);
    }
    free(n_name);
    return ret;
}

syscall_(unlinkat, int dirfd, char *name) {
    if (check_user_overflow((uint64_t)name, strlen(name))) {
        return (uint64_t)-EFAULT;
    }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path)
        return -ENOENT;

    uint64_t ret = syscall_unlink(path, 0, 0, 0, 0, 0, regs);

    free(path);

    return ret;
}

syscall_(access, char *filename) {
    struct stat buf;
    return syscall_stat(filename, &buf, 0, 0, 0, 0, regs);
}

syscall_(mkdir, char *name, uint64_t mode) {
    if (name == NULL)
        return SYSCALL_FAULT_(EINVAL);
    char  *npath = vfs_cwd_path_build(name);
    size_t ret   = vfs_mkdir(npath) == EOK ? EOK : -1;
    if (ret == EOK) {
        vfs_node_t node = vfs_open(npath);
        if (node) {
            uint16_t final_mode = (uint16_t)(mode & 0777);
            final_mode &= (uint16_t)~(get_current_task()->process->umask & 0777);
            vfs_chmod(node, final_mode);
            vfs_close(node);
        }
    }
    free(npath);
    return ret;
}

syscall_(mkdirat, int dirfd, char *name, uint64_t mode) {
    if (name == NULL)
        return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)name, strlen(name)))
        return SYSCALL_FAULT_(EFAULT);
    char *path = at_resolve_pathname(dirfd, name);
    if (!path)
        return SYSCALL_FAULT_(ENOENT);
    size_t ret = vfs_mkdir(path) == EOK ? EOK : -1;
    if (ret == EOK) {
        vfs_node_t node = vfs_open(path);
        if (node) {
            uint16_t final_mode = (uint16_t)(mode & 0777);
            final_mode &= (uint16_t)~(get_current_task()->process->umask & 0777);
            vfs_chmod(node, final_mode);
            vfs_close(node);
        }
    }
    free(path);
    return ret;
}

syscall_(mknod, char *path, uint32_t mode, uint32_t dev) {
    if (path == NULL)
        return SYSCALL_FAULT_(EINVAL);
    char    *npath      = vfs_cwd_path_build(path);
    uint16_t final_mode = (uint16_t)(mode & 07777);
    final_mode &= (uint16_t)~(get_current_task()->process->umask & 0777);
    final_mode |= (uint16_t)(mode & S_IFMT);
    errno_t ret = vfs_mknod(npath, final_mode, (int)dev);
    free(npath);
    return ret == EOK ? EOK : SYSCALL_FAULT_(-ret);
}

syscall_(readlink, char *path, char *buf, uint64_t size) {
    if (path == NULL || buf == NULL || size == 0) {
        return SYSCALL_FAULT_(EINVAL);
    }
    if (check_user_overflow((uint64_t)buf, size)) {
        return SYSCALL_FAULT_(EFAULT);
    }

    char      *npath = vfs_cwd_path_build(path);
    vfs_node_t node  = vfs_open_nofollow(npath);
    free(npath);
    if (node == NULL) {
        return SYSCALL_FAULT_(ENOENT);
    }
    if (!(node->type & file_symlink))
        return SYSCALL_FAULT_(EINVAL);

    return vfs_readlink(node, buf, (size_t)size);
}

syscall_(chmod, char *path, uint64_t mode) {
    if (unlikely(!path || check_user_overflow((uint64_t)path, strlen(path)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char      *npath = vfs_cwd_path_build(path);
    vfs_node_t node  = vfs_open(npath);
    free(npath);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    errno_t ret = vfs_chmod(node, (uint16_t)(mode & 0777));
    vfs_close(node);
    if (ret < 0)
        return SYSCALL_FAULT_(-ret);
    return EOK;
}

syscall_(fchmod, int fd, uint64_t mode) {
    fd_t *fdt = get_fd(get_current_task()->process->fdts, fd);
    if (!fdt || !fdt->node)
        return SYSCALL_FAULT_(EBADF);
    errno_t ret = vfs_chmod(fdt->node, (uint16_t)(mode & 0777));
    if (ret < 0)
        return SYSCALL_FAULT_(-ret);
    return EOK;
}

syscall_(fchmodat, int dirfd, char *path, uint64_t mode, int flags) {
    if (unlikely(!path || check_user_overflow((uint64_t)path, strlen(path)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char *npath = at_resolve_pathname(dirfd, path);
    if (!npath)
        return SYSCALL_FAULT_(ENOENT);
    vfs_node_t node = (flags & 0x100) ? vfs_open_nofollow(npath) : vfs_open(npath);
    free(npath);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    errno_t ret = vfs_chmod(node, (uint16_t)(mode & 0777));
    vfs_close(node);
    if (ret < 0)
        return SYSCALL_FAULT_(-ret);
    return EOK;
}

syscall_(sendfile, int out_fd, int in_fd, uint64_t *offset_ptr, size_t count) {
    pcb_t process    = get_current_task()->process;
    fd_t *out_handle = get_fd(process->fdts, out_fd);
    fd_t *in_handle  = get_fd(process->fdts, in_fd);
    if (out_handle == NULL || in_handle == NULL)
        return SYSCALL_FAULT_(EBADF);

    uint64_t current_offset = offset_ptr == NULL ? in_handle->offset : *offset_ptr;
    size_t   total_sent     = 0;

    size_t remaining = count;

    char *buffer = (char *)malloc(SENDFILE_BUF_SIZE);
    if (buffer == NULL) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    while (remaining > 0) {
        size_t bytes_to_read = remaining < SENDFILE_BUF_SIZE ? remaining : SENDFILE_BUF_SIZE;
        size_t bytes_read;
        size_t bytes_written;
        bytes_read = vfs_read(in_handle->node, buffer, current_offset, bytes_to_read);
        if (bytes_read <= 0) {
            if (bytes_read == (size_t)-1 && total_sent == 0) {
                free(buffer);
                return SYSCALL_FAULT_(EIO);
            }
            break;
        }
        bytes_written = vfs_write(out_handle->node, buffer, out_handle->offset, bytes_read);
        if (bytes_written == (size_t)-1) {
            if (total_sent == 0) {
                free(buffer);
                return SYSCALL_FAULT_(EIO);
            }
            break;
        }
        if (bytes_written < bytes_read) {
            bytes_read = bytes_written;
        }
        current_offset += bytes_read;
        out_handle->offset += bytes_read;
        total_sent += bytes_read;
        remaining -= bytes_read;
    }
    free(buffer);
    if (offset_ptr != NULL) {
        *offset_ptr = current_offset;
    } else {
        in_handle->offset = current_offset;
    }
    return total_sent;
}

syscall_(umask, uint64_t mask) {
    pcb_t    process = get_current_task()->process;
    uint16_t old     = process->umask;
    process->umask   = (uint16_t)(mask & 0777);
    return old;
}

syscall_(openat, int dirfd, char *name, uint64_t flags, uint64_t mode) {
    if (unlikely(!name || check_user_overflow((uint64_t)name, strlen(name)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path)
        return SYSCALL_FAULT_(ENOMEM);
    uint64_t ret = syscall_open(path, flags, mode, 0, 0, 0, regs);
    free(path);
    return ret;
}

syscall_(faccessat, int dirfd, char *pathname, uint64_t mode) {
    if (pathname[0] == '\0') { // by fd
        return 0;
    }
    if (check_user_overflow((uint64_t)pathname, strlen(pathname))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL)
        return SYSCALL_FAULT_(ENOENT);

    size_t ret = syscall_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

syscall_(faccessat2, int dirfd, char *pathname, uint64_t mode, uint64_t flag) {
    if (pathname[0] == '\0') { // by fd
        return 0;
    }
    if (check_user_overflow((uint64_t)pathname, strlen(pathname))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL)
        return SYSCALL_FAULT_(ENOENT);

    size_t ret = syscall_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

syscall_(statfs, char *path, struct statfs *buf) {
    vfs_node_t node = vfs_open(path);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    vfs_filesystem_t filesystem = get_filesystem_node(node);
    if (filesystem == NULL)
        return SYSCALL_FAULT_(EINVAL);
    buf->f_type    = filesystem->magic;
    buf->f_namelen = 255;
    return EOK;
}

syscall_(chroot, char *path) {
    if (path == NULL)
        return SYSCALL_FAULT_(EINVAL);
    char      *npath   = vfs_cwd_path_build(path);
    pcb_t      process = get_current_task()->process;
    vfs_node_t node    = vfs_open(npath);
    free(npath);
    if (node == NULL)
        return SYSCALL_FAULT_(ENOENT);
    if (process->proc_root != NULL)
        vfs_close(process->proc_root);
    process->proc_root = node;
    return EOK;
}

syscall_(chown, const char *filename, uint64_t uid, uint64_t gid) {
    return vfs_chown(filename, uid, gid);
}

syscall_(fchown, int fd, uint64_t uid, uint64_t gid) {
    fd_t *fdt = get_fd(get_current_task()->process->fdts, fd);
    if (!fdt || !fdt->node)
        return SYSCALL_FAULT_(EBADF);
    return EOK;
}

syscall_(lchown, const char *filename, uint64_t uid, uint64_t gid) {
    return vfs_chown(filename, uid, gid);
}

syscall_(fchownat, int dirfd, const char *path, uint64_t uid, uint64_t gid, int flags) {
    if (unlikely(!path || check_user_overflow((uint64_t)path, strlen(path)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char *npath = at_resolve_pathname(dirfd, (char *)path);
    if (!npath)
        return SYSCALL_FAULT_(ENOENT);
    int ret = vfs_chown(npath, uid, gid);
    free(npath);
    return ret;
}

syscall_(utimensat, int dfd, const char *pathname, struct timespec *ntimes, int flags) {
    return EOK;
}

syscall_(futimensat, int dfd, const char *pathname, struct timeval *utimes) {
    return EOK;
}

syscall_(sync) {
    return EOK;
}
