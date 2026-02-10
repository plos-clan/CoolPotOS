#include <fs/fs_syscall.h>
#include <fs/vfs/proc.h>

vfs_node_t epollfs_root;
int epollfs_id;
int epollfd_id = 0;

static int dummy() { return 0; }

// epoll API
size_t epoll_create1(int flags) {
    int i = -1;
    for (i = 0; i < MAX_FD_NUM; i++) {
        if (current_task->fd_info->fds[i] == NULL) {
            break;
        }
    }

    if (i == MAX_FD_NUM) {
        return -EBADF;
    }

    char buf[256];
    sprintf(buf, "epoll%d", epollfd_id++);
    vfs_node_t node = vfs_node_alloc(epollfs_root, buf);
    node->type = file_epoll;
    node->refcount++;
    epoll_t *epoll = malloc(sizeof(epoll_t));
    epoll->lock.lock = 0;
    llist_init_head(&epoll->watches);
    node->mode = 0700;
    node->handle = epoll;
    node->fsid = epollfs_id;

    current_task->fd_info->fds[i] = malloc(sizeof(fd_t));
    memset(current_task->fd_info->fds[i], 0, sizeof(fd_t));
    current_task->fd_info->fds[i]->node = node;
    current_task->fd_info->fds[i]->offset = 0;
    current_task->fd_info->fds[i]->flags = flags;
    procfs_on_open_file(current_task, i);

    return i;
}

uint64_t sys_epoll_create(int size) { return epoll_create1(0); }

uint64_t epoll_wait(vfs_node_t epollFd, struct epoll_event *events,
                    int maxevents, int64_t timeout) {
    if (maxevents < 1)
        return (uint64_t)-EINVAL;
    if (!epollFd || epollFd->fsid != epollfs_id)
        return (uint64_t)-EBADF;

    epoll_t *epoll = epollFd->handle;
    if (!epoll)
        return -EINVAL;

    int ready = 0;
    uint64_t target_timeout = 0;

    if (timeout > 0) {
        target_timeout = nano_time() + timeout;
    } else if (timeout == 0) {
        goto check_events;
    }

    bool timed_out = false;
    bool interrupted = false;

    while (ready == 0 && !timed_out && !interrupted) {
        if (signals_pending_quick(current_task)) {
            interrupted = true;
            break;
        }

        if (timeout > 0 && nano_time() >= target_timeout) {
            timed_out = true;
            break;
        }

    check_events:
        spin_lock(&epoll->lock);

        ready = 0;

        epoll_watch_t *browse, *tmp;
        llist_for_each(browse, tmp, &epoll->watches, node) {
            if (ready < maxevents) {
                if (!browse->fd) {
                    continue;
                }
                if (!browse->fd->node->handle) {
                    continue;
                }

                uint32_t current_events =
                    vfs_poll(browse->fd->node, browse->events);

                uint32_t ready_events = current_events & browse->events;

                if (ready_events) {
                    // 处理边缘触发逻辑
                    if (browse->edge_trigger) {
                        // 只返回新出现的事件
                        uint32_t new_events =
                            ready_events & ~browse->last_events;
                        if (new_events) {
                            events[ready].events = new_events;
                            events[ready].data.u64 = browse->data;
                            ready++;
                            browse->last_events = ready_events; // 更新状态
                        } else {
                            spin_unlock(&epoll->lock);
                            goto ret;
                        }
                    } else {
                        // 水平触发：只要事件存在就返回
                        events[ready].events = ready_events;
                        events[ready].data.u64 = browse->data;
                        ready++;
                    }
                } else if (!browse->edge_trigger) {
                    browse->last_events = 0;
                }

                continue;
            } else {
                break;
            }
        }

        spin_unlock(&epoll->lock);

        if (ready > 0)
            break;

        if (timeout != 0) {
            schedule(SCHED_FLAG_YIELD);
        } else {
            goto ret;
        }
    }

    if (interrupted)
        return (uint64_t)-EINTR;

ret:
    return ready;
}

size_t epoll_ctl(vfs_node_t epollFd, int op, int fd,
                 struct epoll_event *event) {
    if (op != EPOLL_CTL_ADD && op != EPOLL_CTL_DEL && op != EPOLL_CTL_MOD)
        return (uint64_t)(-EINVAL);

    if (!epollFd || epollFd->fsid != epollfs_id)
        return (uint64_t)(-EINVAL);

    epoll_t *epoll = epollFd->handle;
    if (!epoll)
        return -EINVAL;

    // 检查文件描述符有效性
    if (fd < 0 || fd >= MAX_FD_NUM || !current_task->fd_info->fds[fd]) {
        return (uint64_t)(-EBADF);
    }

    fd_t *f = current_task->fd_info->fds[fd];

    spin_lock(&epoll->lock);

    epoll_watch_t *existing = NULL;
    epoll_watch_t *b, *t;
    llist_for_each(b, t, &epoll->watches, node) {
        if (b->fd && (b->fd->node == f->node)) {
            existing = b;
            break;
        }
    }

    int ret = 0;

    switch (op) {
    case EPOLL_CTL_ADD:
        if (existing) {
            ret = -EEXIST;
            break;
        }

        epoll_watch_t *new_watch = malloc(sizeof(epoll_watch_t));
        if (!new_watch) {
            ret = -ENOMEM;
            break;
        }

        new_watch->fd = f;
        new_watch->events = event->events & ~EPOLLET;
        new_watch->data = event->data.u64;
        // new_watch->edge_trigger = (event->events & EPOLLET) != 0;
        new_watch->edge_trigger = false;
        new_watch->last_events = 0;
        llist_init_head(&new_watch->node);
        llist_append(&epoll->watches, &new_watch->node);

        // 移除EPOLLET标志，因为它不是真正的事件
        new_watch->events &= ~EPOLLET;
        break;

    case EPOLL_CTL_DEL:
        if (!existing) {
            ret = -ENOENT;
            break;
        }

        llist_delete(&existing->node);

        free(existing);
        break;

    case EPOLL_CTL_MOD:
        if (!existing) {
            ret = -ENOENT;
            break;
        }

        existing->events = event->events & ~EPOLLET;
        existing->data = event->data.u64;
        // existing->edge_trigger = (event->events & EPOLLET) != 0;
        existing->edge_trigger = false;
        existing->last_events = 0; // 重置状态
        break;

    default:
        ret = -EINVAL;
        break;
    }

    spin_unlock(&epoll->lock);
    return ret;
}

size_t epoll_pwait(vfs_node_t epollFd, struct epoll_event *events,
                   int maxevents, int64_t timeout, sigset_t *sigmask,
                   size_t sigsetsize) {
    if (check_user_overflow((uint64_t)events,
                            maxevents * sizeof(struct epoll_event))) {
        return (uint64_t)-EFAULT;
    }

    sigset_t origmask;
    if (sigmask)
        sys_ssetmask(SIG_SETMASK, sigmask, &origmask, sizeof(sigset_t));
    size_t epollRet = epoll_wait(epollFd, events, maxevents, timeout * 1000000);
    if (sigmask)
        sys_ssetmask(SIG_SETMASK, &origmask, 0, sizeof(sigset_t));

    return epollRet;
}

uint64_t sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents,
                        int timeout) {
    if (check_user_overflow((uint64_t)events,
                            maxevents * sizeof(struct epoll_event))) {
        return (uint64_t)-EFAULT;
    }
    vfs_node_t node = current_task->fd_info->fds[epfd]->node;
    if (!node)
        return (uint64_t)-EBADF;
    return epoll_wait(node, events, maxevents, timeout);
}

uint64_t sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    if (check_user_overflow((uint64_t)event, sizeof(struct epoll_event))) {
        return (uint64_t)-EFAULT;
    }
    if (epfd >= MAX_FD_NUM || current_task->fd_info->fds[epfd] == NULL) {
        return (uint64_t)-EBADF;
    }
    vfs_node_t node = current_task->fd_info->fds[epfd]->node;
    if (!node)
        return (uint64_t)-EBADF;
    return epoll_ctl(node, op, fd, event);
}

uint64_t sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                         int timeout, sigset_t *sigmask, size_t sigsetsize) {
    if (check_user_overflow((uint64_t)events,
                            maxevents * sizeof(struct epoll_event))) {
        return (uint64_t)-EFAULT;
    }
    if (epfd >= MAX_FD_NUM || current_task->fd_info->fds[epfd] == NULL) {
        return (uint64_t)-EBADF;
    }
    vfs_node_t node = current_task->fd_info->fds[epfd]->node;
    if (!node)
        return (uint64_t)-EBADF;
    return epoll_pwait(node, events, maxevents, timeout, sigmask, sigsetsize);
}

uint64_t sys_epoll_pwait2(int epfd, struct epoll_event *events, int maxevents,
                          struct timespec *timeout, sigset_t *sigmask,
                          size_t sigsetsize) {
    if (check_user_overflow((uint64_t)events,
                            maxevents * sizeof(struct epoll_event))) {
        return (uint64_t)-EFAULT;
    }
    if (epfd >= MAX_FD_NUM || current_task->fd_info->fds[epfd] == NULL) {
        return (uint64_t)-EBADF;
    }
    vfs_node_t node = current_task->fd_info->fds[epfd]->node;
    if (!node)
        return (uint64_t)-EBADF;
    uint64_t timeout_ns;
    if (!timeout) {
        timeout_ns = -1;
    } else {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
            timeout->tv_nsec >= 1000000000LL) {
            return (uint64_t)-EINVAL;
        }
        timeout_ns = timeout->tv_sec * 1000000000LL + timeout->tv_nsec;
    }
    return epoll_pwait(node, events, maxevents, timeout_ns, sigmask,
                       sigsetsize);
}

uint64_t sys_epoll_create1(int flags) { return epoll_create1(flags); }

bool epollfs_close(void *current) {
    // if (!epoll->reference_count)
    //     return true;
    return false;
}

static int epoll_poll(void *file, size_t event) {
    epoll_t *epoll = file;
    int revents = 0;

    epoll_watch_t *browse, *tmp;
    llist_for_each(browse, tmp, &epoll->watches, node) {
        if (!browse->fd)
            continue;
        if (!browse->fd->node->handle)
            continue;
        int ret = vfs_poll(browse->fd->node, event);
        if (ret) {
            revents |= EPOLLIN;
            break;
        }
    }

    return revents;
}

static struct vfs_callback epoll_callbacks = {
    .mount = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .remount = (vfs_remount_t)dummy,
    .open = (vfs_open_t)dummy,
    .close = epollfs_close,
    .read = (vfs_read_t)dummy,
    .write = (vfs_write_t)dummy,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir = (vfs_mk_t)dummy,
    .mkfile = (vfs_mk_t)dummy,
    .link = (vfs_mk_t)dummy,
    .symlink = (vfs_mk_t)dummy,
    .mknod = (vfs_mknod_t)dummy,
    .chmod = (vfs_chmod_t)dummy,
    .chown = (vfs_chown_t)dummy,
    .rename = (vfs_rename_t)dummy,
    .delete = (vfs_del_t)dummy,
    .map = (vfs_mapfile_t)dummy,
    .stat = (vfs_stat_t)dummy,
    .ioctl = (vfs_ioctl_t)dummy,
    .poll = epoll_poll,
    .resize = (vfs_resize_t)dummy,

    .free_handle = vfs_generic_free_handle,
};

fs_t epollfs = {
    .name = "epollfs",
    .magic = 0,
    .callback = &epoll_callbacks,
    .flags = FS_FLAGS_HIDDEN,
};

void epoll_init() {
    epollfs_id = vfs_regist(&epollfs);
    epollfs_root = vfs_node_alloc(NULL, "epoll");
    epollfs_root->type = file_dir;
    epollfs_root->mode = 0644;
    epollfs_root->fsid = epollfs_id;
}
