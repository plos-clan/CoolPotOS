#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/fds.h"
#include "fs/sockfs.h"
#include "fs/vfs.h"
#include "syscall.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

extern vfs_node_t sockfs_root;
extern int sockfs_id;

static void trace_x11_sock_event(const char *op, int fd, ssize_t ret, size_t size, int flags) {
    const tcb_t current = get_current_task();
    if (current == NULL || current->process == NULL || current->process->name == NULL) {
        return;
    }
    if (!strstr(current->process->name, "xinit") && !strstr(current->process->name, "Xorg")) {
        return;
    }
    logkf(
        "[sock-dbg] proc=%s pid=%d op=%s fd=%d ret=%d size=%d flags=0x%x\n",
        current->process->name,
        current->process->pid,
        op,
        fd,
        (int)ret,
        (int)size,
        flags
    );
}

static void trace_x11_sockaddr(const char *op, int fd, const struct sockaddr_un *sun, socklen_t addrlen) {
    const tcb_t current = get_current_task();
    if (current == NULL || current->process == NULL || current->process->name == NULL || sun == NULL) {
        return;
    }
    if (!strstr(current->process->name, "xinit") && !strstr(current->process->name, "Xorg")) {
        return;
    }
    if (sun->sun_path[0] == '\0') {
        logkf(
            "[sockaddr-dbg] proc=%s pid=%d op=%s fd=%d len=%d path=@%s\n",
            current->process->name,
            current->process->pid,
            op,
            fd,
            (int)addrlen,
            sun->sun_path + 1
        );
        return;
    }
    logkf(
        "[sockaddr-dbg] proc=%s pid=%d op=%s fd=%d len=%d path=%s\n",
        current->process->name,
        current->process->pid,
        op,
        fd,
        (int)addrlen,
        sun->sun_path
    );
}

static void destroy_socket_info(socket_info_t *info) {
    if (!info)
        return;
    ringbuf_destroy(&info->recv_buf);
    free(info);
}

static void fill_cred_from_current(socket_info_t *info) {
    if (!info || !get_current_task() || !get_current_task()->process)
        return;
    info->cred.pid = get_current_task()->process->pid;
    info->cred.uid = get_current_task()->process->euid;
    info->cred.gid = get_current_task()->process->egid;
}

static void snapshot_peer_cred(socket_info_t *info, const socket_info_t *peer) {
    if (!info || !peer)
        return;
    info->peer_cred     = peer->cred;
    info->has_peer_cred = true;
}

static bool get_peer_cred(const socket_info_t *info, struct ucred *cred) {
    if (!info || !cred)
        return false;
    if (info->peer) {
        *cred = info->peer->cred;
        return true;
    }
    if (!info->has_peer_cred)
        return false;
    *cred = info->peer_cred;
    return true;
}

static void copy_local_name(socket_info_t *dst, const socket_info_t *src) {
    if (!dst || !src || !src->is_bound)
        return;
    dst->is_bound         = true;
    dst->bound_abstract   = src->bound_abstract;
    dst->bound_registered = false;
    dst->bound_node       = NULL;
    strncpy(dst->bound_path, src->bound_path, UNIX_PATH_MAX - 1);
    dst->bound_path[UNIX_PATH_MAX - 1] = '\0';
}

static bool is_x11_display_socket(const struct sockaddr_un *sun) {
    static const char x11_prefix[] = "/tmp/.X11-unix/X";
    if (!sun)
        return false;

    if (sun->sun_path[0] == '\0')
        return strncmp(sun->sun_path + 1, x11_prefix, sizeof(x11_prefix) - 1) == 0;

    char *path = vfs_cwd_path_build((char *)sun->sun_path);
    if (!path)
        return false;
    const bool match = strncmp(path, x11_prefix, sizeof(x11_prefix) - 1) == 0;
    free(path);
    return match;
}

static socket_info_t *alloc_socket_info(int domain, int type, int protocol) {
    socket_info_t *info = calloc(1, sizeof(socket_info_t));
    if (!info)
        return NULL;
    info->domain   = domain;
    info->type     = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    info->protocol = protocol;
    info->state    = SS_UNCONNECTED;
    info->refcount = 1;
    info->lock     = SPIN_INIT;
    ringbuf_init(&info->recv_buf, SOCK_BUFF);
    fill_cred_from_current(info);
    return info;
}

static int sock_create_fd(socket_info_t *info, int flags) {
    vfs_node_t node = sockfs_create_node(info);
    if (!node)
        return -ENOMEM;

    fd_t *handle   = calloc(1, sizeof(fd_t));
    asserts(handle,"sock_create_fd: handle is null.");
    handle->node   = node;
    handle->offset = 0;
    handle->flags  = 0;
    if (flags & SOCK_NONBLOCK)
        handle->flags |= O_NONBLOCK;
    if (flags & SOCK_CLOEXEC)
        handle->flags |= O_CLOEXEC;
    int fd     = add_fd(get_current_task()->process->fdts, handle);
    handle->fd = fd;
    return fd;
}

static socket_specific_t *get_sock_spec(int fd) {
    fd_t *handle = get_fd(get_current_task()->process->fdts, fd);
    if (!handle)
        return NULL;
    if (!(handle->node->type & file_socket))
        return NULL;
    return (socket_specific_t *)handle->node->handle;
}

syscall_(socket, int domain, int type, int protocol) {
    if (domain != AF_UNIX)
        return SYSCALL_FAULT_(EAFNOSUPPORT);
    int real_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (real_type != SOCK_STREAM && real_type != SOCK_DGRAM)
        return SYSCALL_FAULT_(EPROTOTYPE);

    if (sockfs_root == NULL)
        return SYSCALL_FAULT_(ENOSYS);

    socket_info_t *info = alloc_socket_info(domain, type, protocol);
    if (!info)
        return SYSCALL_FAULT_(ENOMEM);

    int fd = sock_create_fd(info, type);
    if (fd < 0) {
        ringbuf_destroy(&info->recv_buf);
        free(info);
        return SYSCALL_FAULT_(ENOMEM);
    }
    return fd;
}

syscall_(socketpair, int domain, int type, int protocol, int *sv) {
    if (!sv)
        return SYSCALL_FAULT_(EINVAL);
    if (domain != AF_UNIX)
        return SYSCALL_FAULT_(EAFNOSUPPORT);
    int real_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (real_type != SOCK_STREAM && real_type != SOCK_DGRAM)
        return SYSCALL_FAULT_(EPROTOTYPE);

    if (sockfs_root == NULL)
        return SYSCALL_FAULT_(ENOSYS);

    socket_info_t *info0 = alloc_socket_info(domain, type, protocol);
    socket_info_t *info1 = alloc_socket_info(domain, type, protocol);
    if (!info0 || !info1) {
        if (info0) {
            ringbuf_destroy(&info0->recv_buf);
            free(info0);
        }
        if (info1) {
            ringbuf_destroy(&info1->recv_buf);
            free(info1);
        }
        return SYSCALL_FAULT_(ENOMEM);
    }

    // Cross-link peers
    info0->peer  = info1;
    info1->peer  = info0;
    info0->state = SS_CONNECTED;
    info1->state = SS_CONNECTED;
    snapshot_peer_cred(info0, info1);
    snapshot_peer_cred(info1, info0);

    int fd0 = sock_create_fd(info0, type);
    int fd1 = sock_create_fd(info1, type);
    if (fd0 < 0 || fd1 < 0) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    sv[0] = fd0;
    sv[1] = fd1;
    return EOK;
}

syscall_(bind, int sockfd, struct sockaddr *addr, uint64_t addrlen) {
    if (!addr)
        return trace_x11_sock_event("bind", sockfd, -EINVAL, 0, 0), SYSCALL_FAULT_(EINVAL);
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("bind", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("bind", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);

    if (addr->sa_family != AF_UNIX)
        return trace_x11_sock_event("bind", sockfd, -EAFNOSUPPORT, 0, 0), SYSCALL_FAULT_(EAFNOSUPPORT);
    errno_t ret = sockfs_bind_endpoint(info, (struct sockaddr_un *)addr, addrlen);
    if (ret < 0)
        return trace_x11_sock_event("bind", sockfd, ret, 0, 0), SYSCALL_FAULT_(-ret);
    trace_x11_sock_event("bind", sockfd, EOK, 0, 0);
    return EOK;
}

syscall_(listen, int sockfd, int backlog) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("listen", sockfd, -ENOTSOCK, 0, backlog), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("listen", sockfd, -ENOTSOCK, 0, backlog), SYSCALL_FAULT_(ENOTSOCK);
    if (!info->bound_registered)
        return trace_x11_sock_event("listen", sockfd, -EINVAL, 0, backlog), SYSCALL_FAULT_(EINVAL);
    if (info->type != SOCK_STREAM)
        return trace_x11_sock_event("listen", sockfd, -EOPNOTSUPP, 0, backlog), SYSCALL_FAULT_(EOPNOTSUPP);

    if (backlog <= 0)
        backlog = 5;
    if (backlog > 128)
        backlog = 128;

    spin_lock(info->lock);
    if (info->pending_queue) {
        free(info->pending_queue);
        info->pending_queue = NULL;
    }
    info->backlog       = backlog;
    info->pending_queue = calloc(backlog, sizeof(socket_info_t *));
    info->pending_count = 0;
    info->state         = SS_LISTENING;
    spin_unlock(info->lock);
    sockfs_notify(info, EPOLLOUT);
    trace_x11_sock_event("listen", sockfd, EOK, 0, backlog);
    return EOK;
}

static uint64_t socket_accept_common(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC))
        return trace_x11_sock_event("accept", sockfd, -EINVAL, 0, flags), SYSCALL_FAULT_(EINVAL);

    fd_t *listener_handle = get_fd(get_current_task()->process->fdts, sockfd);
    if (!listener_handle)
        return trace_x11_sock_event("accept", sockfd, -EBADF, 0, flags), SYSCALL_FAULT_(EBADF);
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("accept", sockfd, -ENOTSOCK, 0, flags), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("accept", sockfd, -ENOTSOCK, 0, flags), SYSCALL_FAULT_(ENOTSOCK);
    if (info->state != SS_LISTENING)
        return trace_x11_sock_event("accept", sockfd, -EINVAL, 0, flags), SYSCALL_FAULT_(EINVAL);

    // Wait for a pending connection
    for (;;) {
        spin_lock(info->lock);
        if (info->pending_count > 0) {
            socket_info_t *server_info = info->pending_queue[0];
            info->pending_count--;
            for (int i = 0; i < info->pending_count; i++)
                info->pending_queue[i] = info->pending_queue[i + 1];
            spin_unlock(info->lock);
            sockfs_notify(info, EPOLLOUT);

            // Create fd for the new server endpoint
            int new_fd = sock_create_fd(server_info, flags);
            if (new_fd < 0) {
                if (server_info->peer) {
                    socket_info_t *peer = server_info->peer;
                    spin_lock(peer->lock);
                    if (peer->peer == server_info)
                        peer->peer = NULL;
                    spin_unlock(peer->lock);
                    sockfs_notify(peer, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
                }
                destroy_socket_info(server_info);
                return SYSCALL_FAULT_(ENOMEM);
            }

            // Fill in addr if requested
            if (addr && addrlen) {
                sockfs_fill_sockaddr(server_info->peer, (struct sockaddr_un *)addr, addrlen);
            }
            trace_x11_sock_event("accept", sockfd, new_fd, 0, listener_handle->flags);
            return new_fd;
        }
        spin_unlock(info->lock);
        if (listener_handle->flags & O_NONBLOCK) {
            trace_x11_sock_event("accept", sockfd, -EAGAIN, 0, listener_handle->flags);
            return SYSCALL_FAULT_(EAGAIN);
        }
        scheduler_yield();
    }
}

syscall_(accept, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return socket_accept_common(sockfd, addr, addrlen, 0);
}

syscall_(accept4, int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    return socket_accept_common(sockfd, addr, addrlen, flags);
}

syscall_(connect, int sockfd, struct sockaddr *addr, uint64_t addrlen) {
    if (!addr)
        return trace_x11_sock_event("connect", sockfd, -EINVAL, 0, 0), SYSCALL_FAULT_(EINVAL);
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("connect", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("connect", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);

    if (addr->sa_family != AF_UNIX)
        return trace_x11_sock_event("connect", sockfd, -EAFNOSUPPORT, 0, 0), SYSCALL_FAULT_(EAFNOSUPPORT);
    struct sockaddr_un *sun = (struct sockaddr_un *)addr;
    fd_t *fd_handle         = get_fd(get_current_task()->process->fdts, sockfd);
    if (!fd_handle)
        return trace_x11_sock_event("connect", sockfd, -EBADF, 0, 0), SYSCALL_FAULT_(EBADF);

    if (info->type == SOCK_DGRAM) {
        socket_info_t *target_info = NULL;
        bool path_exists           = false;
        errno_t ret = sockfs_lookup_bound(sun, addrlen, &target_info, &path_exists);
        if (ret < 0)
            return trace_x11_sock_event("connect-dgram", sockfd, ret, 0, fd_handle->flags), SYSCALL_FAULT_(-ret);
        if (!target_info)
            return trace_x11_sock_event(
                       "connect-dgram", sockfd, -(path_exists ? ECONNREFUSED : ENOENT), 0, fd_handle->flags
                   ),
                   SYSCALL_FAULT_(path_exists ? ECONNREFUSED : ENOENT);

        spin_lock(info->lock);
        info->peer  = target_info;
        info->state = SS_CONNECTED;
        snapshot_peer_cred(info, target_info);
        spin_unlock(info->lock);
        trace_x11_sock_event("connect-dgram", sockfd, EOK, 0, fd_handle->flags);
        return EOK;
    }

    // SOCK_STREAM connect
    if (info->state == SS_CONNECTED)
        return trace_x11_sock_event("connect", sockfd, -EISCONN, 0, fd_handle->flags), SYSCALL_FAULT_(EISCONN);
    if (info->state == SS_CONNECTING)
        return trace_x11_sock_event("connect", sockfd, -EALREADY, 0, fd_handle->flags), SYSCALL_FAULT_(EALREADY);
    const bool wait_for_x11_listener = !(fd_handle->flags & O_NONBLOCK) && is_x11_display_socket(sun);

    for (;;) {
        socket_info_t *listener_info = NULL;
        bool path_exists             = false;
        errno_t ret = sockfs_lookup_bound(sun, addrlen, &listener_info, &path_exists);
        if (ret < 0)
            return trace_x11_sock_event("connect", sockfd, ret, 0, fd_handle->flags),
                   SYSCALL_FAULT_(-ret);
        if (!listener_info) {
            if (wait_for_x11_listener) {
                scheduler_yield();
                continue;
            }
            trace_x11_sock_event(
                "connect", sockfd, -(path_exists ? ECONNREFUSED : ENOENT), 0, fd_handle->flags
            );
            return SYSCALL_FAULT_(path_exists ? ECONNREFUSED : ENOENT);
        }

        spin_lock(listener_info->lock);
        if (listener_info->state != SS_LISTENING || !listener_info->pending_queue) {
            spin_unlock(listener_info->lock);
            if (wait_for_x11_listener) {
                scheduler_yield();
                continue;
            }
            trace_x11_sock_event("connect", sockfd, -ECONNREFUSED, 0, fd_handle->flags);
            return SYSCALL_FAULT_(ECONNREFUSED);
        }
        if (listener_info->pending_count < listener_info->backlog) {
            socket_info_t *server_info =
                alloc_socket_info(listener_info->domain, listener_info->type, listener_info->protocol);
            if (!server_info) {
                spin_unlock(listener_info->lock);
                trace_x11_sock_event("connect", sockfd, -ENOMEM, 0, fd_handle->flags);
                return SYSCALL_FAULT_(ENOMEM);
            }

            server_info->cred     = listener_info->cred;
            server_info->passcred = listener_info->passcred;
            server_info->peer     = info;
            server_info->state    = SS_CONNECTED;
            copy_local_name(server_info, listener_info);
            snapshot_peer_cred(server_info, info);

            spin_lock(info->lock);
            info->peer  = server_info;
            info->state = SS_CONNECTED;
            snapshot_peer_cred(info, server_info);
            spin_unlock(info->lock);

            listener_info->pending_queue[listener_info->pending_count++] = server_info;
            spin_unlock(listener_info->lock);

            sockfs_notify(listener_info, EPOLLIN);
            sockfs_notify(info, EPOLLOUT);
            trace_x11_sock_event("connect", sockfd, EOK, 0, fd_handle->flags);
            return EOK;
        }
        spin_unlock(listener_info->lock);

        if (fd_handle->flags & O_NONBLOCK) {
            trace_x11_sock_event("connect", sockfd, -EAGAIN, 0, fd_handle->flags);
            return SYSCALL_FAULT_(EAGAIN);
        }
        scheduler_yield();
    }
}

syscall_(shutdown, const int sockfd, const int how) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (how < SHUT_RD || how > SHUT_RDWR)
        return SYSCALL_FAULT_(EINVAL);

    spin_lock(info->lock);
    if (how == SHUT_RD || how == SHUT_RDWR)
        info->shut_rd = true;
    if (how == SHUT_WR || how == SHUT_RDWR)
        info->shut_wr = true;
    socket_info_t *peer = info->peer;
    spin_unlock(info->lock);

    sockfs_notify(info, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    if (peer)
        sockfs_notify(peer, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    trace_x11_sock_event("shutdown", sockfd, EOK, 0, how);
    return EOK;
}

syscall_(
    sendto,
    const int sockfd,
    const void *buf,
    const size_t len,
    const int flags,
    struct sockaddr *dest_addr,
    uint64_t addrlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    socket_info_t *info = spec->info;
    if (!info) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    fd_t *fd_handle = get_fd(get_current_task()->process->fdts, sockfd);
    if (!fd_handle)
        return SYSCALL_FAULT_(EBADF);
    if (info->shut_wr) {
        return SYSCALL_FAULT_(EPIPE);
    }

    socket_info_t *target = info->peer;

    // For DGRAM with dest_addr, find the target
    if (info->type == SOCK_DGRAM && dest_addr) {
        struct sockaddr_un *sun = (struct sockaddr_un *)dest_addr;
        if (sun->sun_family == AF_UNIX) {
            socket_info_t *target_info = NULL;
            if (sockfs_lookup_bound(sun, addrlen, &target_info, NULL) == EOK && target_info) {
                target = target_info;
            }
        }
    }

    if (!target) {
        return SYSCALL_FAULT_(ENOTCONN);
    }

    // Write data to target's recv_buf
    const uint8_t *src = buf;
    size_t total       = 0;
    size_t remaining   = len;

    while (remaining > 0) {
        spin_lock(target->lock);
        if (target->closed || target->shut_rd) {
            spin_unlock(target->lock);
            return total > 0 ? total : SYSCALL_FAULT_(EPIPE);
        }
        const size_t avail = target->recv_buf.capacity - target->recv_buf.count;
        if (avail > 0) {
            size_t to_write = MIN(remaining, avail);
            ringbuf_write(&target->recv_buf, src + total, to_write);
            total += to_write;
            remaining -= to_write;
            spin_unlock(target->lock);
            sockfs_notify(target, EPOLLIN);
            if (info->type == SOCK_DGRAM) {
                break; // DGRAM: single write
            }
        } else {
            spin_unlock(target->lock);
            if ((flags & MSG_DONTWAIT) || (fd_handle->flags & O_NONBLOCK)) {
                break;
            }
            scheduler_yield();
        }
    }
    const ssize_t ret = total > 0 ? (ssize_t)total : -(ssize_t)EAGAIN;
    if (ret <= 0 || len <= 64)
        trace_x11_sock_event("sendto", sockfd, ret, len, flags);
    return ret > 0 ? (uint64_t)ret : SYSCALL_FAULT_(-ret);
}

syscall_(
    recvfrom,
    const int sockfd,
    void *buf,
    const size_t len,
    const int flags,
    struct sockaddr *src_addr,
    socklen_t *addrlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    socket_info_t *info = spec->info;
    if (!info) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    fd_t *fd_handle = get_fd(get_current_task()->process->fdts, sockfd);
    if (!fd_handle)
        return SYSCALL_FAULT_(EBADF);
    if (info->shut_rd) {
        return 0;
    }

    for (;;) {
        spin_lock(info->lock);
        if (info->recv_buf.count > 0) {
            size_t to_read = MIN(len, info->recv_buf.count);
            if (flags & MSG_PEEK)
                ringbuf_peek(&info->recv_buf, buf, to_read);
            else
                ringbuf_read(&info->recv_buf, buf, to_read);
            socket_info_t *peer = info->peer;
            spin_unlock(info->lock);
            sockfs_notify(peer, EPOLLOUT);

            if (src_addr && addrlen) {
                sockfs_fill_sockaddr(info->peer, (struct sockaddr_un *)src_addr, addrlen);
            }
            if (to_read <= 64)
                trace_x11_sock_event("recvfrom", sockfd, to_read, len, flags);
            return to_read;
        }
        // Check for EOF (stream peer gone)
        if (info->type == SOCK_STREAM
            && (info->peer == NULL || info->peer->closed || info->peer->shut_wr)
            && info->state == SS_CONNECTED) {
            spin_unlock(info->lock);
            trace_x11_sock_event("recvfrom", sockfd, 0, len, flags);
            return 0;
        }
        spin_unlock(info->lock);
        if ((flags & MSG_DONTWAIT) || (fd_handle->flags & O_NONBLOCK)) {
            trace_x11_sock_event("recvfrom", sockfd, -EAGAIN, len, flags | (fd_handle->flags & O_NONBLOCK));
            return SYSCALL_FAULT_(EAGAIN);
        }
        scheduler_yield();
    }
}

syscall_(sendmsg, const int sockfd, struct msghdr *msg, const int flags) {
    if (!msg) {
        return SYSCALL_FAULT_(EINVAL);
    }
    size_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        const struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0) {
            continue;
        }
        size_t ret = syscall_sendto(
            sockfd, iov->iov_base, iov->iov_len, flags, msg->msg_name, msg->msg_namelen, regs
        );
        if ((int64_t)ret < 0) {
            return total > 0 ? total : ret;
        }
        total += ret;
    }
    return total;
}

syscall_(recvmsg, const int sockfd, struct msghdr *msg, const int flags) {
    if (!msg) {
        return SYSCALL_FAULT_(EINVAL);
    }
    size_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0) {
            continue;
        }
        size_t ret = syscall_recvfrom(sockfd, iov->iov_base, iov->iov_len, flags, NULL, NULL, regs);
        if ((int64_t)ret < 0) {
            return total > 0 ? total : ret;
        }
        total += ret;
        if (ret < iov->iov_len) {
            break; // short read
        }
    }
    msg->msg_controllen = 0;
    msg->msg_flags      = 0;
    return total;
}

static uint64_t copy_sockopt_value(void *optval, socklen_t *optlen, const void *value, size_t value_len) {
    size_t copy_len = MIN(*optlen, value_len);
    memcpy(optval, value, copy_len);
    *optlen = (socklen_t)value_len;
    return EOK;
}

syscall_(
    setsockopt,
    const int sockfd,
    const int level,
    const int optname,
    const void *optval,
    const uint64_t optlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (level != SOL_SOCKET)
        return SYSCALL_FAULT_(ENOPROTOOPT);

    switch (optname) {
    case SO_PASSCRED:
        if (!optval || optlen < sizeof(int))
            return SYSCALL_FAULT_(EINVAL);
        info->passcred = *(const int *)optval;
        return EOK;
    case SO_SNDBUF:
    case SO_RCVBUF:
    case SO_REUSEADDR:
    case SO_KEEPALIVE:
        return EOK;
    default:
        return SYSCALL_FAULT_(ENOPROTOOPT);
    }
}

syscall_(
    getsockopt, const int sockfd, const int level, const int optname, void *optval, socklen_t *optlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    socket_info_t *info = spec->info;
    if (!info) {
        return SYSCALL_FAULT_(ENOTSOCK);
    }
    if (!optval || !optlen) {
        return SYSCALL_FAULT_(EINVAL);
    }

    if (level == SOL_SOCKET) {
        int val = 0;
        switch (optname) {
        case SO_ERROR:
            val            = info->so_error;
            info->so_error = 0;
            break;
        case SO_TYPE:
            val = info->type;
            break;
        case SO_DOMAIN:
            val = info->domain;
            break;
        case SO_ACCEPTCONN:
            val = info->state == SS_LISTENING ? 1 : 0;
            break;
        case SO_RCVBUF:
            val = (int)info->recv_buf.capacity;
            break;
        case SO_SNDBUF:
            val = SOCK_BUFF;
            break;
        case SO_PASSCRED:
            val = info->passcred;
            break;
        case SO_PEERCRED: {
            struct ucred peer_cred = {0};
            if (!get_peer_cred(info, &peer_cred))
                return SYSCALL_FAULT_(ENOTCONN);
            trace_x11_sock_event("getsockopt", sockfd, EOK, sizeof(peer_cred), optname);
            return copy_sockopt_value(optval, optlen, &peer_cred, sizeof(peer_cred));
        }
        default:
            return SYSCALL_FAULT_(ENOPROTOOPT);
        }
        if (optname == SO_SNDBUF || optname == SO_ERROR || optname == SO_ACCEPTCONN)
            trace_x11_sock_event("getsockopt", sockfd, EOK, sizeof(val), optname);
        return copy_sockopt_value(optval, optlen, &val, sizeof(val));
    }
    return SYSCALL_FAULT_(ENOPROTOOPT);
}

syscall_(getsockname, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("getsockname", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("getsockname", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    if (!addr || !addrlen)
        return trace_x11_sock_event("getsockname", sockfd, -EINVAL, 0, 0), SYSCALL_FAULT_(EINVAL);

    sockfs_fill_sockaddr(info, (struct sockaddr_un *)addr, addrlen);
    trace_x11_sockaddr("getsockname", sockfd, (struct sockaddr_un *)addr, *addrlen);
    trace_x11_sock_event("getsockname", sockfd, EOK, *addrlen, 0);
    return EOK;
}

syscall_(getpeername, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return trace_x11_sock_event("getpeername", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return trace_x11_sock_event("getpeername", sockfd, -ENOTSOCK, 0, 0), SYSCALL_FAULT_(ENOTSOCK);
    if (!addr || !addrlen)
        return trace_x11_sock_event("getpeername", sockfd, -EINVAL, 0, 0), SYSCALL_FAULT_(EINVAL);
    if (!info->peer)
        return trace_x11_sock_event("getpeername", sockfd, -ENOTCONN, 0, 0), SYSCALL_FAULT_(ENOTCONN);

    sockfs_fill_sockaddr(info->peer, (struct sockaddr_un *)addr, addrlen);
    trace_x11_sockaddr("getpeername", sockfd, (struct sockaddr_un *)addr, *addrlen);
    trace_x11_sock_event("getpeername", sockfd, EOK, *addrlen, 0);
    return EOK;
}
