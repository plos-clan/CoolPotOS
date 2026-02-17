#include "task/poll.h"
#include "errno.h"
#include "fs/fds.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "syscall.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

uint32_t epoll_to_poll_comp(uint32_t epoll_events) {
    uint32_t poll_events = 0;

    if (epoll_events & EPOLLIN) {
        poll_events |= POLLIN;
    }
    if (epoll_events & EPOLLOUT) {
        poll_events |= POLLOUT;
    }
    if (epoll_events & EPOLLPRI) {
        poll_events |= POLLPRI;
    }
    if (epoll_events & EPOLLERR) {
        poll_events |= POLLERR;
    }
    if (epoll_events & EPOLLHUP) {
        poll_events |= POLLHUP;
    }

    return poll_events;
}

uint32_t poll_to_epoll_comp(uint32_t poll_events) {
    uint32_t epoll_events = 0;

    if (poll_events & POLLIN) {
        epoll_events |= EPOLLIN;
    }
    if (poll_events & POLLOUT) {
        epoll_events |= EPOLLOUT;
    }
    if (poll_events & POLLPRI) {
        epoll_events |= EPOLLPRI;
    }
    if (poll_events & POLLERR) {
        epoll_events |= EPOLLERR;
    }
    if (poll_events & POLLHUP) {
        epoll_events |= EPOLLHUP;
    }

    return epoll_events;
}

struct pollfd *
select_add(struct pollfd **comp, size_t *compIndex, size_t *complength, int fd, int events) {
    if ((*compIndex + 1) * sizeof(struct pollfd) >= *complength) {
        *complength *= 2;
        *comp = realloc(*comp, *complength);
    }

    (*comp)[*compIndex].fd = fd;
    (*comp)[*compIndex].events = events;
    (*comp)[*compIndex].revents = 0;

    return &(*comp)[(*compIndex)++];
}

bool select_bitmap(const uint8_t *map, int index) {
    int div = index / 8;
    int mod = index % 8;
    return map[div] & (1 << mod);
}

void select_bitmap_set(uint8_t *map, int index) {
    int div = index / 8;
    int mod = index % 8;
    map[div] |= 1 << mod;
}

// ============================================================
// epollfs - epoll 文件系统实现
// ============================================================

static vfs_node_t epollfs_root = NULL;
static int epollfs_id = 0;
static int epollfd_id = 0;

// --- epollfs VFS callbacks ---

static bool epollfs_close(void *current) {
    epoll_instance_t *ep = (epoll_instance_t *)current;
    if (!ep)
        return true;
    if (ep->node)
        ep->node->handle = NULL;
    free(ep);
    return true;
}

static int epollfs_poll(void *file, size_t events) {
    epoll_instance_t *ep = (epoll_instance_t *)file;
    if (!ep)
        return 0;

    extern vfs_callback_t fs_callbacks[256];
    int out = 0;
    tcb_t current = get_current_task();
    fdt_t *fdt = current->process->fdts;

    spin_lock(ep->lock);
    for (int i = 0; i < ep->count; i++) {
        fd_t *handle = get_fd(fdt, ep->entries[i].fd);
        if (!handle)
            continue;
        vfs_node_t node = handle->node;
        if (fs_callbacks[node->fsid]->poll == (void *)dummy) {
            if (events & EPOLLIN) {
                out |= EPOLLIN;
                break;
            }
            continue;
        }
        int revents = vfs_poll(node, ep->entries[i].events);
        if (revents > 0) {
            if (events & EPOLLIN)
                out |= EPOLLIN;
            break;
        }
    }
    spin_unlock(ep->lock);
    return out;
}

static errno_t epollfs_free(void *handle) {
    (void)handle;
    return EOK;
}

static struct vfs_callback epollfs_callbacks = {
    .mount = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open = (vfs_open_t)dummy,
    .close = epollfs_close,
    .read = (vfs_read_t)dummy,
    .write = (vfs_write_t)dummy,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir = (vfs_mk_t)dummy,
    .mkfile = (vfs_mk_t)dummy,
    .link = (vfs_mk_t)dummy,
    .symlink = (vfs_mk_t)dummy,
    .delete = (vfs_del_t)dummy,
    .rename = (vfs_rename_t)dummy,
    .map = (vfs_mapfile_t)dummy,
    .stat = (vfs_stat_t)dummy,
    .ioctl = (vfs_ioctl_t)dummy,
    .poll = epollfs_poll,
    .dup = (vfs_dup_t)dummy,
    .free = epollfs_free,
    .chmod = (vfs_chmod_t)dummy,
    .mknod = (vfs_mknod_t)dummy,
};

void epollfs_regist() {
    epollfs_id = vfs_regist("epollfs", &epollfs_callbacks, 0x45504F4C, FS_VIRTUAL_FLAGS);
    if (epollfs_id == -EINVAL) {
        kerror("epollfs regist error.");
    }
    epollfs_root = vfs_node_alloc(rootdir, ".epollfs");
    epollfs_root->type = file_dir;
    epollfs_root->fsid = epollfs_id;
}

// ============================================================
// epoll 系统调用实现
// ============================================================

syscall_(epoll_create1, int flags) {
    if (flags & ~O_CLOEXEC)
        return SYSCALL_FAULT_(EINVAL);
    if (!epollfs_root)
        return SYSCALL_FAULT_(ENOMEM);

    epoll_instance_t *ep = calloc(1, sizeof(epoll_instance_t));
    if (!ep)
        return SYSCALL_FAULT_(ENOMEM);

    char buf[20];
    sprintf(buf, "epoll%d", epollfd_id++);
    vfs_node_t node = vfs_node_alloc(epollfs_root, buf);
    node->type = file_epoll;
    node->fsid = epollfs_id;
    node->mode = 0700;
    node->handle = ep;
    ep->node = node;

    fd_t *handle = calloc(1, sizeof(fd_t));
    handle->node = node;
    handle->offset = 0;
    handle->flags = flags & O_CLOEXEC ? O_CLOEXEC : 0;

    fdt_t *fdt = get_current_task()->process->fdts;
    int fd = add_fd(fdt, handle);
    handle->fd = fd;

    return (uint64_t)fd;
}

syscall_(epoll_ctl, int epfd, int op, int fd, struct epoll_event *event) {
    fdt_t *fdt = get_current_task()->process->fdts;

    fd_t *ep_handle = get_fd(fdt, epfd);
    if (!ep_handle)
        return SYSCALL_FAULT_(EBADF);
    if (!(ep_handle->node->type & file_epoll))
        return SYSCALL_FAULT_(EINVAL);

    epoll_instance_t *ep = (epoll_instance_t *)ep_handle->node->handle;
    if (!ep)
        return SYSCALL_FAULT_(EBADF);

    // Validate target fd exists
    fd_t *target = get_fd(fdt, fd);
    if (!target)
        return SYSCALL_FAULT_(EBADF);

    // Cannot add epoll fd to itself
    if (fd == epfd)
        return SYSCALL_FAULT_(EINVAL);

    spin_lock(ep->lock);

    // Find existing entry for this fd
    int found = -1;
    for (int i = 0; i < ep->count; i++) {
        if (ep->entries[i].fd == fd) {
            found = i;
            break;
        }
    }

    switch (op) {
    case EPOLL_CTL_ADD:
        if (found >= 0) {
            spin_unlock(ep->lock);
            return SYSCALL_FAULT_(EEXIST);
        }
        if (ep->count >= EPOLL_MAX_ENTRIES) {
            spin_unlock(ep->lock);
            return SYSCALL_FAULT_(ENOMEM);
        }
        ep->entries[ep->count].fd = fd;
        ep->entries[ep->count].events = event->events;
        ep->entries[ep->count].data = event->data;
        ep->count++;
        break;

    case EPOLL_CTL_DEL:
        if (found < 0) {
            spin_unlock(ep->lock);
            return SYSCALL_FAULT_(ENOENT);
        }
        // Swap with last entry
        ep->entries[found] = ep->entries[ep->count - 1];
        ep->count--;
        break;

    case EPOLL_CTL_MOD:
        if (found < 0) {
            spin_unlock(ep->lock);
            return SYSCALL_FAULT_(ENOENT);
        }
        ep->entries[found].events = event->events;
        ep->entries[found].data = event->data;
        break;

    default:
        spin_unlock(ep->lock);
        return SYSCALL_FAULT_(EINVAL);
    }

    spin_unlock(ep->lock);
    return 0;
}

syscall_(epoll_wait, int epfd, struct epoll_event *events, int maxevents, int timeout) {
    if (maxevents <= 0 || !events)
        return SYSCALL_FAULT_(EINVAL);

    tcb_t current = get_current_task();
    fdt_t *fdt = current->process->fdts;
    fd_t *ep_handle = get_fd(fdt, epfd);
    if (!ep_handle)
        return SYSCALL_FAULT_(EBADF);
    if (!(ep_handle->node->type & file_epoll))
        return SYSCALL_FAULT_(EINVAL);

    epoll_instance_t *ep = (epoll_instance_t *)ep_handle->node->handle;
    if (!ep)
        return SYSCALL_FAULT_(EBADF);

    extern vfs_callback_t fs_callbacks[256];
    uint64_t start_time = nano_time();
    int ready = 0;

    do {
        ready = 0;
        spin_lock(ep->lock);

        for (int i = 0; i < ep->count && ready < maxevents; i++) {
            fd_t *handle = get_fd(fdt, ep->entries[i].fd);
            if (!handle)
                continue;

            vfs_node_t node = handle->node;
            uint32_t revents;

            if (fs_callbacks[node->fsid]->poll == (void *)dummy) {
                // Filesystem doesn't implement poll - assume ready
                revents = ep->entries[i].events & (EPOLLIN | EPOLLOUT);
            } else {
                revents = (uint32_t)vfs_poll(node, ep->entries[i].events | EPOLLERR | EPOLLHUP);
            }

            if (revents > 0) {
                events[ready].events = revents;
                events[ready].data = ep->entries[i].data;
                ready++;
            }
        }

        spin_unlock(ep->lock);

        if (ready > 0)
            return (uint64_t)ready;

        if (signals_pending_quick(current))
            return SYSCALL_FAULT_(EINTR);

        if (timeout == 0)
            break;

        scheduler_yield();
    } while (timeout < 0 || (nano_time() - start_time) < (uint64_t)timeout * 1000000ULL);

    return 0;
}

syscall_(
    epoll_pwait, int epfd, struct epoll_event *events, int maxevents, int timeout,
    sigset_t *sigmask, size_t sigsetsize) {
    tcb_t thread = get_current_task();
    sigset_t old_mask = thread->blocked;

    if (sigmask && sigsetsize == sizeof(sigset_t)) {
        thread->blocked = *sigmask;
    }

    uint64_t ret = syscall_epoll_wait(epfd, events, maxevents, timeout, 0, 0, regs);

    if (sigmask && sigsetsize == sizeof(sigset_t)) {
        thread->blocked = old_mask;
    }

    return ret;
}

// ============================================================
// eventfdfs - eventfd 文件系统实现
// ============================================================

static vfs_node_t eventfdfs_root = NULL;
static int eventfdfs_id = 0;
static int eventfd_nid = 0;

static size_t eventfdfs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)file;
    if (!ctx || size < sizeof(uint64_t))
        return (size_t)-1;
    tcb_t current = get_current_task();

    for (;;) {
        spin_lock(ctx->lock);
        if (ctx->count > 0) {
            uint64_t val;
            if (ctx->flags & EFD_SEMAPHORE) {
                val = 1;
                ctx->count--;
            } else {
                val = ctx->count;
                ctx->count = 0;
            }
            spin_unlock(ctx->lock);
            *(uint64_t *)addr = val;
            return sizeof(uint64_t);
        }
        spin_unlock(ctx->lock);

        if (ctx->flags & EFD_NONBLOCK)
            return (size_t)-1;

        if (signals_pending_quick(current))
            return (size_t)-1;

        scheduler_yield();
    }
}

static size_t eventfdfs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)file;
    if (!ctx || size < sizeof(uint64_t))
        return (size_t)-1;
    tcb_t current = get_current_task();

    uint64_t val = *(const uint64_t *)addr;
    if (val == UINT64_MAX)
        return (size_t)-1;

    for (;;) {
        spin_lock(ctx->lock);
        if (ctx->count <= UINT64_MAX - 2 - val) {
            ctx->count += val;
            spin_unlock(ctx->lock);
            return sizeof(uint64_t);
        }
        spin_unlock(ctx->lock);

        if (ctx->flags & EFD_NONBLOCK)
            return (size_t)-1;

        if (signals_pending_quick(current))
            return (size_t)-1;

        scheduler_yield();
    }
}

static bool eventfdfs_close(void *current) {
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)current;
    if (!ctx)
        return true;
    if (ctx->node)
        ctx->node->handle = NULL;
    free(ctx);
    return true;
}

static int eventfdfs_poll(void *file, size_t events) {
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)file;
    if (!ctx)
        return 0;

    int out = 0;
    spin_lock(ctx->lock);
    if ((events & EPOLLIN) && ctx->count > 0)
        out |= EPOLLIN;
    if ((events & EPOLLOUT) && ctx->count < UINT64_MAX - 1)
        out |= EPOLLOUT;
    spin_unlock(ctx->lock);
    return out;
}

static struct vfs_callback eventfdfs_callbacks = {
    .mount = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open = (vfs_open_t)dummy,
    .close = eventfdfs_close,
    .read = eventfdfs_read,
    .write = eventfdfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir = (vfs_mk_t)dummy,
    .mkfile = (vfs_mk_t)dummy,
    .link = (vfs_mk_t)dummy,
    .symlink = (vfs_mk_t)dummy,
    .delete = (vfs_del_t)dummy,
    .rename = (vfs_rename_t)dummy,
    .map = (vfs_mapfile_t)dummy,
    .stat = (vfs_stat_t)dummy,
    .ioctl = (vfs_ioctl_t)dummy,
    .poll = eventfdfs_poll,
    .dup = (vfs_dup_t)dummy,
    .free = (vfs_free_t)dummy,
    .chmod = (vfs_chmod_t)dummy,
    .mknod = (vfs_mknod_t)dummy,
};

void eventfdfs_regist() {
    eventfdfs_id = vfs_regist("eventfdfs", &eventfdfs_callbacks, 0x45564644, FS_VIRTUAL_FLAGS);
    if (eventfdfs_id == -EINVAL) {
        kerror("eventfdfs regist error.");
    }
    eventfdfs_root = vfs_node_alloc(rootdir, ".eventfdfs");
    eventfdfs_root->type = file_dir;
    eventfdfs_root->fsid = eventfdfs_id;
}

// ============================================================
// eventfd2 系统调用实现
// ============================================================

syscall_(eventfd2, uint64_t initval, int flags) {
    if (flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE))
        return SYSCALL_FAULT_(EINVAL);
    if (!eventfdfs_root)
        return SYSCALL_FAULT_(ENOMEM);

    eventfd_ctx_t *ctx = calloc(1, sizeof(eventfd_ctx_t));
    if (!ctx)
        return SYSCALL_FAULT_(ENOMEM);

    ctx->count = initval;
    ctx->flags = flags;

    char buf[24];
    sprintf(buf, "eventfd%d", eventfd_nid++);
    vfs_node_t node = vfs_node_alloc(eventfdfs_root, buf);
    node->type = file_eventfd;
    node->fsid = eventfdfs_id;
    node->mode = 0700;
    node->handle = ctx;
    ctx->node = node;

    fd_t *handle = calloc(1, sizeof(fd_t));
    handle->node = node;
    handle->offset = 0;
    handle->flags = 0;
    if (flags & EFD_CLOEXEC)
        handle->flags |= O_CLOEXEC;
    if (flags & EFD_NONBLOCK)
        handle->flags |= O_NONBLOCK;

    fdt_t *fdt = get_current_task()->process->fdts;
    int fd = add_fd(fdt, handle);
    handle->fd = fd;

    return (uint64_t)fd;
}
