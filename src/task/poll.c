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

#define EPOLL_ALWAYS_EVENTS (EPOLLERR | EPOLLHUP | EPOLLNVAL)

static inline uint32_t epoll_filter_events(uint32_t events) {
    return events & ~(EPOLLET | EPOLLONESHOT | EPOLLWAKEUP | EPOLLEXCLUSIVE);
}

static uint32_t epoll_query_ready_events(epoll_entry_t *entry, fd_t *handle) {
    if (!entry || !handle || !handle->node) {
        return EPOLLNVAL;
    }

    extern vfs_callback_t fs_callbacks[256];
    const vfs_node_t node   = handle->node;
    uint32_t request_events = entry->events | EPOLL_ALWAYS_EVENTS;

    if (fs_callbacks[node->fsid]->poll == (void *)dummy) {
        return request_events & (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLNVAL);
    }

    uint32_t revents = (uint32_t)vfs_poll(node, request_events);
    revents |= (uint32_t)vfs_poll(node, EPOLL_ALWAYS_EVENTS) & EPOLL_ALWAYS_EVENTS;
    return revents;
}

static bool epoll_should_emit(epoll_entry_t *entry, uint32_t ready_events) {
    if (!entry || entry->disabled || !ready_events) {
        return false;
    }

    if (entry->edge_trigger) {
        uint32_t state_events = ready_events & ~EPOLL_ALWAYS_EVENTS;
        uint32_t raised       = state_events & ~entry->last_events;
        entry->last_events    = state_events;
        return raised != 0 || (ready_events & EPOLL_ALWAYS_EVENTS) != 0;
    }

    entry->last_events = ready_events & ~EPOLL_ALWAYS_EVENTS;
    return true;
}

static bool epoll_would_emit(const epoll_entry_t *entry, uint32_t ready_events) {
    if (!entry || entry->disabled || !ready_events) {
        return false;
    }

    if (entry->edge_trigger) {
        uint32_t state_events = ready_events & ~EPOLL_ALWAYS_EVENTS;
        uint32_t raised       = state_events & ~entry->last_events;
        return raised != 0 || (ready_events & EPOLL_ALWAYS_EVENTS) != 0;
    }

    return true;
}

static size_t epoll_arm_waiters_locked(
    epoll_instance_t *ep, const fdt_t *fdt, tcb_t current, vfs_poll_wait_t *waits, size_t max_waits
) {
    size_t count = 0;

    if (!ep || !fdt || !current || !waits || max_waits == 0) {
        return 0;
    }

    for (int i = 0; i < ep->count && count < max_waits; i++) {
        if (ep->entries[i].disabled) {
            continue;
        }

        fd_t *handle = get_fd(fdt, ep->entries[i].fd);
        if (!handle || !handle->node) {
            continue;
        }

        vfs_poll_wait_init(&waits[count], current, ep->entries[i].events | EPOLL_ALWAYS_EVENTS);
        if (vfs_poll_wait_arm(handle->node, &waits[count]) == EOK) {
            count++;
        }
    }

    return count;
}

static void epoll_disarm_waiters(vfs_poll_wait_t *waits, size_t count) {
    if (!waits) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (waits[i].armed) {
            vfs_poll_wait_disarm(&waits[i]);
        }
    }
}

uint32_t epoll_to_poll_comp(const uint32_t epoll_events) {
    uint32_t poll_events = 0;

    if (epoll_events & (EPOLLIN | EPOLLRDNORM)) {
        poll_events |= POLLIN | POLLRDNORM;
    }
    if (epoll_events & (EPOLLPRI | EPOLLRDBAND)) {
        poll_events |= POLLPRI | POLLRDBAND;
    }
    if (epoll_events & (EPOLLOUT | EPOLLWRNORM)) {
        poll_events |= POLLOUT | POLLWRNORM;
    }
    if (epoll_events & EPOLLWRBAND) {
        poll_events |= POLLWRBAND;
    }
    if (epoll_events & EPOLLERR) {
        poll_events |= POLLERR;
    }
    if (epoll_events & EPOLLHUP) {
        poll_events |= POLLHUP;
    }
    if (epoll_events & EPOLLNVAL) {
        poll_events |= POLLNVAL;
    }
    if (epoll_events & EPOLLRDHUP) {
        poll_events |= POLLRDHUP;
    }

    return poll_events;
}

uint32_t poll_to_epoll_comp(const uint32_t poll_events) {
    uint32_t epoll_events = 0;

    if (poll_events & (POLLIN | POLLRDNORM)) {
        epoll_events |= EPOLLIN | EPOLLRDNORM;
    }
    if (poll_events & (POLLPRI | POLLRDBAND)) {
        epoll_events |= EPOLLPRI | EPOLLRDBAND;
    }
    if (poll_events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
        epoll_events |= EPOLLOUT | EPOLLWRNORM;
        if (poll_events & POLLWRBAND) {
            epoll_events |= EPOLLWRBAND;
        }
    }
    if (poll_events & POLLERR) {
        epoll_events |= EPOLLERR;
    }
    if (poll_events & POLLHUP) {
        epoll_events |= EPOLLHUP;
    }
    if (poll_events & POLLNVAL) {
        epoll_events |= EPOLLNVAL;
    }
    if (poll_events & POLLRDHUP) {
        epoll_events |= EPOLLRDHUP;
    }

    return epoll_events;
}

struct pollfd *select_add(
    struct pollfd **comp, size_t *compIndex, size_t *complength, const int fd, const int events
) {
    if ((*compIndex + 1) * sizeof(struct pollfd) >= *complength) {
        *complength *= 2;
        *comp = realloc(*comp, *complength);
    }

    (*comp)[*compIndex].fd      = fd;
    (*comp)[*compIndex].events  = (short)events;
    (*comp)[*compIndex].revents = 0;

    return &(*comp)[(*compIndex)++];
}

bool select_bitmap(const uint8_t *map, const int index) {
    const int div = index / 8;
    const int mod = index % 8;
    return map[div] & 1 << mod;
}

void select_bitmap_set(uint8_t *map, const int index) {
    const int div = index / 8;
    const int mod = index % 8;
    map[div] |= 1 << mod;
}

static vfs_node_t epollfs_root = NULL;
static int epollfs_id          = 0;
static int epollfd_id          = 0;

static bool epollfs_close(void *current) {
    epoll_instance_t *ep = current;
    if (!ep) {
        return true;
    }
    if (ep->node) {
        ep->node->handle = NULL;
    }
    free(ep);
    return true;
}

static int epollfs_poll(void *file, size_t events) {
    epoll_instance_t *ep = file;
    if (!ep) {
        return 0;
    }

    extern vfs_callback_t fs_callbacks[256];
    int out             = 0;
    const tcb_t current = get_current_task();
    fdt_t *fdt          = current->process->fdts;

    spin_lock(ep->lock);
    for (int i = 0; i < ep->count; i++) {
        if (ep->entries[i].disabled) {
            continue;
        }

        fd_t *handle = get_fd(fdt, ep->entries[i].fd);
        if (!handle) {
            continue;
        }

        uint32_t revents = epoll_query_ready_events(&ep->entries[i], handle);
        if (revents > 0 && epoll_would_emit(&ep->entries[i], revents)) {
            if (events & EPOLLIN) {
                out |= EPOLLIN;
            }
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
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)dummy,
    .close    = epollfs_close,
    .read     = (vfs_read_t)dummy,
    .write    = (vfs_write_t)dummy,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = (vfs_stat_t)dummy,
    .ioctl    = (vfs_ioctl_t)dummy,
    .poll     = epollfs_poll,
    .dup      = (vfs_dup_t)dummy,
    .free     = epollfs_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void epollfs_regist() {
    epollfs_id = vfs_regist("epollfs", &epollfs_callbacks, 0x45504F4C, FS_VIRTUAL_FLAGS);
    if (epollfs_id == -EINVAL) {
        kerror("epollfs regist error.");
    }
    epollfs_root       = vfs_node_alloc(rootdir, ".epollfs");
    epollfs_root->type = file_dir;
    epollfs_root->fsid = epollfs_id;
}

syscall_(epoll_create1, const int flags) {
    if (flags & ~O_CLOEXEC) {
        return SYSCALL_FAULT_(EINVAL);
    }
    if (!epollfs_root) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    epoll_instance_t *ep = calloc(1, sizeof(epoll_instance_t));
    if (!ep) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    char buf[20];
    sprintf(buf, "epoll%d", epollfd_id++);
    const vfs_node_t node = vfs_node_alloc(epollfs_root, buf);
    node->type            = file_epoll;
    node->fsid            = epollfs_id;
    node->mode            = 0700;
    node->handle          = ep;
    ep->node              = node;

    fd_t *handle = calloc(1, sizeof(fd_t));
    asserts(handle, "syscall_epoll_create1: handle is null.");
    handle->node   = node;
    handle->offset = 0;
    handle->flags  = flags & O_CLOEXEC ? O_CLOEXEC : 0;

    fdt_t *fdt   = get_current_task()->process->fdts;
    const int fd = add_fd(fdt, handle);
    handle->fd   = fd;

    return (uint64_t)fd;
}

syscall_(epoll_ctl, const int epfd, const int op, const int fd, const struct epoll_event *event) {
    fdt_t *fdt = get_current_task()->process->fdts;

    fd_t *ep_handle = get_fd(fdt, epfd);
    if (!ep_handle) {
        return SYSCALL_FAULT_(EBADF);
    }
    if (!(ep_handle->node->type & file_epoll)) {
        return SYSCALL_FAULT_(EINVAL);
    }

    epoll_instance_t *ep = ep_handle->node->handle;
    if (!ep) {
        return SYSCALL_FAULT_(EBADF);
    }

    // Validate target fd exists
    fd_t *target = get_fd(fdt, fd);
    if (!target) {
        return SYSCALL_FAULT_(EBADF);
    }

    // Cannot add epoll fd to itself
    if (fd == epfd) {
        return SYSCALL_FAULT_(EINVAL);
    }

    if ((op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) && !event) {
        return SYSCALL_FAULT_(EFAULT);
    }

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
        ep->entries[ep->count].fd           = fd;
        ep->entries[ep->count].events       = epoll_filter_events(event->events);
        ep->entries[ep->count].data         = event->data;
        ep->entries[ep->count].last_events  = 0;
        ep->entries[ep->count].edge_trigger = !!(event->events & EPOLLET);
        ep->entries[ep->count].one_shot     = !!(event->events & EPOLLONESHOT);
        ep->entries[ep->count].disabled     = false;
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
        ep->entries[found].events       = epoll_filter_events(event->events);
        ep->entries[found].data         = event->data;
        ep->entries[found].last_events  = 0;
        ep->entries[found].edge_trigger = !!(event->events & EPOLLET);
        ep->entries[found].one_shot     = !!(event->events & EPOLLONESHOT);
        ep->entries[found].disabled     = false;
        break;

    default:
        spin_unlock(ep->lock);
        return SYSCALL_FAULT_(EINVAL);
    }

    spin_unlock(ep->lock);
    return 0;
}

syscall_(
    epoll_wait, const int epfd, struct epoll_event *events, const int maxevents, const int timeout
) {
    if (maxevents <= 0 || !events) {
        return SYSCALL_FAULT_(EINVAL);
    }

    const tcb_t current = get_current_task();
    fdt_t *fdt          = current->process->fdts;
    fd_t *ep_handle     = get_fd(fdt, epfd);
    if (!ep_handle) {
        return SYSCALL_FAULT_(EBADF);
    }
    if (!(ep_handle->node->type & file_epoll)) {
        return SYSCALL_FAULT_(EINVAL);
    }

    epoll_instance_t *ep = ep_handle->node->handle;
    if (!ep) {
        return SYSCALL_FAULT_(EBADF);
    }

    const uint64_t start_time   = nano_time();
    const bool infinite_timeout = timeout < 0;
    const uint64_t timeout_ns   = infinite_timeout ? 0 : (uint64_t)timeout * 1000000ULL;
    vfs_poll_wait_t *waits      = calloc(EPOLL_MAX_ENTRIES, sizeof(*waits));
    int ready                   = 0;

    if (!waits) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    arch_open_interrupt();
    scheduler_enable();
    do {
        ready = 0;
        spin_lock(ep->lock);

        for (int i = 0; i < ep->count && ready < maxevents; i++) {
            if (ep->entries[i].disabled) {
                continue;
            }

            fd_t *handle = get_fd(fdt, ep->entries[i].fd);
            if (!handle) {
                continue;
            }

            uint32_t revents = epoll_query_ready_events(&ep->entries[i], handle);
            if (revents > 0 && epoll_should_emit(&ep->entries[i], revents)) {
                events[ready].events = revents;
                events[ready].data   = ep->entries[i].data;
                if (ep->entries[i].one_shot) {
                    ep->entries[i].disabled = true;
                }
                ready++;
            } else if (ep->entries[i].edge_trigger && revents == 0) {
                ep->entries[i].last_events = 0;
            }
        }

        spin_unlock(ep->lock);

        if (ready > 0) {
            free(waits);
            return (uint64_t)ready;
        }

        if (signals_pending_quick(current)) {
            free(waits);
            return SYSCALL_FAULT_(EINTR);
        }

        if (timeout == 0) {
            break;
        }

        size_t wait_count = 0;
        spin_lock(ep->lock);
        wait_count = epoll_arm_waiters_locked(ep, fdt, current, waits, EPOLL_MAX_ENTRIES);
        spin_unlock(ep->lock);

        if (wait_count > 0) {
            ready = 0;
            spin_lock(ep->lock);
            for (int i = 0; i < ep->count && ready < maxevents; i++) {
                if (ep->entries[i].disabled) {
                    continue;
                }

                fd_t *handle = get_fd(fdt, ep->entries[i].fd);
                if (!handle) {
                    continue;
                }

                uint32_t revents = epoll_query_ready_events(&ep->entries[i], handle);
                if (revents > 0 && epoll_should_emit(&ep->entries[i], revents)) {
                    events[ready].events = revents;
                    events[ready].data   = ep->entries[i].data;
                    if (ep->entries[i].one_shot) {
                        ep->entries[i].disabled = true;
                    }
                    ready++;
                } else if (ep->entries[i].edge_trigger && revents == 0) {
                    ep->entries[i].last_events = 0;
                }
            }
            spin_unlock(ep->lock);

            if (ready == 0 && !signals_pending_quick(current)) {
                int64_t wait_ns = -1;
                if (!infinite_timeout) {
                    uint64_t elapsed = nano_time() - start_time;
                    if (elapsed >= timeout_ns) {
                        wait_ns = 0;
                    } else {
                        wait_ns = (int64_t)(timeout_ns - elapsed);
                    }
                }

                int64_t block_ns = wait_ns;
                if (block_ns < 0 || block_ns > 10000000LL) {
                    block_ns = 10000000LL;
                }
                scheduler_block_current((uint64_t)block_ns, "epoll_wait");
            }

            epoll_disarm_waiters(waits, wait_count);

            if (ready > 0) {
                free(waits);
                return (uint64_t)ready;
            }
            if (signals_pending_quick(current)) {
                free(waits);
                return SYSCALL_FAULT_(EINTR);
            }
        } else {
            scheduler_yield();
        }
    } while (infinite_timeout || nano_time() - start_time < timeout_ns);
    arch_close_interrupt();
    free(waits);

    return 0;
}

syscall_(
    epoll_pwait,
    const int epfd,
    struct epoll_event *events,
    const int maxevents,
    const int timeout,
    const sigset_t *sigmask,
    const size_t sigsetsize
) {
    const tcb_t thread      = get_current_task();
    const sigset_t old_mask = thread->blocked;

    if (sigmask && sigsetsize == sizeof(sigset_t)) {
        thread->blocked = sigset_user_to_kernel(*sigmask);
    }

    const uint64_t ret = syscall_epoll_wait(epfd, events, maxevents, timeout, 0, 0, regs);

    if (sigmask && sigsetsize == sizeof(sigset_t)) {
        thread->blocked = old_mask;
    }

    return ret;
}

static vfs_node_t eventfdfs_root = NULL;
static int eventfdfs_id          = 0;
static int eventfd_nid           = 0;

static size_t eventfdfs_read(void *file, void *addr, const size_t offset, const size_t size) {
    (void)offset;
    eventfd_ctx_t *ctx = file;
    if (!ctx || size < sizeof(uint64_t)) {
        return (size_t)-1;
    }
    const tcb_t current = get_current_task();

    for (;;) {
        spin_lock(ctx->lock);
        if (ctx->count > 0) {
            uint64_t val;
            if (ctx->flags & EFD_SEMAPHORE) {
                val = 1;
                ctx->count--;
            } else {
                val        = ctx->count;
                ctx->count = 0;
            }
            spin_unlock(ctx->lock);
            *(uint64_t *)addr = val;
            return sizeof(uint64_t);
        }
        spin_unlock(ctx->lock);

        if (ctx->flags & EFD_NONBLOCK) {
            return (size_t)-1;
        }

        if (signals_pending_quick(current)) {
            return (size_t)-1;
        }

        scheduler_yield();
    }
}

static size_t
eventfdfs_write(void *file, const void *addr, const size_t offset, const size_t size) {
    (void)offset;
    eventfd_ctx_t *ctx = file;
    if (!ctx || size < sizeof(uint64_t)) {
        return (size_t)-1;
    }
    const tcb_t current = get_current_task();

    const uint64_t val = *(const uint64_t *)addr;
    if (val == UINT64_MAX) {
        return (size_t)-1;
    }

    for (;;) {
        spin_lock(ctx->lock);
        if (ctx->count <= UINT64_MAX - 2 - val) {
            ctx->count += val;
            spin_unlock(ctx->lock);
            return sizeof(uint64_t);
        }
        spin_unlock(ctx->lock);

        if (ctx->flags & EFD_NONBLOCK) {
            return (size_t)-1;
        }

        if (signals_pending_quick(current)) {
            return (size_t)-1;
        }

        scheduler_yield();
    }
}

static bool eventfdfs_close(void *current) {
    eventfd_ctx_t *ctx = current;
    if (!ctx) {
        return true;
    }
    if (ctx->node) {
        ctx->node->handle = NULL;
    }
    free(ctx);
    return true;
}

static int eventfdfs_poll(void *file, const size_t events) {
    eventfd_ctx_t *ctx = file;
    if (!ctx) {
        return 0;
    }

    int out = 0;
    spin_lock(ctx->lock);
    if (events & EPOLLIN && ctx->count > 0) {
        out |= EPOLLIN;
    }
    if (events & EPOLLOUT && ctx->count < UINT64_MAX - 1) {
        out |= EPOLLOUT;
    }
    spin_unlock(ctx->lock);
    return out;
}

static struct vfs_callback eventfdfs_callbacks = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)dummy,
    .close    = eventfdfs_close,
    .read     = eventfdfs_read,
    .write    = eventfdfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = (vfs_stat_t)dummy,
    .ioctl    = (vfs_ioctl_t)dummy,
    .poll     = eventfdfs_poll,
    .dup      = (vfs_dup_t)dummy,
    .free     = (vfs_free_t)dummy,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void eventfdfs_regist() {
    eventfdfs_id = vfs_regist("eventfdfs", &eventfdfs_callbacks, 0x45564644, FS_VIRTUAL_FLAGS);
    if (eventfdfs_id == -EINVAL) {
        kerror("eventfdfs regist error.");
    }
    eventfdfs_root       = vfs_node_alloc(rootdir, ".eventfdfs");
    eventfdfs_root->type = file_dir;
    eventfdfs_root->fsid = eventfdfs_id;
}

syscall_(eventfd2, const uint64_t initval, const int flags) {
    if (flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)) {
        return SYSCALL_FAULT_(EINVAL);
    }
    if (!eventfdfs_root) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    eventfd_ctx_t *ctx = calloc(1, sizeof(eventfd_ctx_t));
    if (!ctx) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    ctx->count = initval;
    ctx->flags = flags;

    char buf[24];
    sprintf(buf, "eventfd%d", eventfd_nid++);
    const vfs_node_t node = vfs_node_alloc(eventfdfs_root, buf);
    node->type            = file_eventfd;
    node->fsid            = eventfdfs_id;
    node->mode            = 0700;
    node->handle          = ctx;
    ctx->node             = node;

    fd_t *handle = calloc(1, sizeof(fd_t));
    asserts(handle, "syscall_eventfd2: handle is null.");
    handle->node   = node;
    handle->offset = 0;
    handle->flags  = 0;
    if (flags & EFD_CLOEXEC) {
        handle->flags |= O_CLOEXEC;
    }
    if (flags & EFD_NONBLOCK) {
        handle->flags |= O_NONBLOCK;
    }

    fdt_t *fdt   = get_current_task()->process->fdts;
    const int fd = add_fd(fdt, handle);
    handle->fd   = fd;

    return (uint64_t)fd;
}
