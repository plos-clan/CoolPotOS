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
    return info;
}

static int sock_create_fd(socket_info_t *info, int flags) {
    vfs_node_t node = sockfs_create_node(info);
    if (!node)
        return -ENOMEM;

    fd_t *handle   = calloc(1, sizeof(fd_t));
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
        return SYSCALL_FAULT_(EINVAL);
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);

    if (addr->sa_family != AF_UNIX)
        return SYSCALL_FAULT_(EAFNOSUPPORT);
    struct sockaddr_un *sun = (struct sockaddr_un *)addr;
    if (sun->sun_path[0] == '\0') {
        // Abstract socket - just mark as bound
        spin_lock(info->lock);
        info->is_bound = true;
        memcpy(info->bound_path, sun->sun_path, UNIX_PATH_MAX);
        info->state = SS_BOUND;
        spin_unlock(info->lock);
        return EOK;
    }

    char *path = vfs_cwd_path_build(sun->sun_path);
    if (!path)
        return SYSCALL_FAULT_(ENOMEM);

    // Check if path already exists
    vfs_node_t existing = vfs_open(path);
    if (existing) {
        free(path);
        return SYSCALL_FAULT_(EADDRINUSE);
    }

    // Create a VFS node at the path for the socket
    errno_t ret = vfs_mkfile(path);
    if (ret != EOK) {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_node_t node = vfs_open(path);
    if (!node) {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    node->type = file_socket;

    spin_lock(info->lock);
    strncpy(info->bound_path, path, UNIX_PATH_MAX - 1);
    info->bound_path[UNIX_PATH_MAX - 1] = '\0';
    info->is_bound                      = true;
    info->bound_node                    = node;
    info->state                         = SS_BOUND;
    // Store info pointer in the VFS node so connect() can find it
    node->handle = spec;
    spin_unlock(info->lock);

    free(path);
    return EOK;
}

syscall_(listen, int sockfd, int backlog) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (!info->is_bound)
        return SYSCALL_FAULT_(EINVAL);
    if (info->type != SOCK_STREAM)
        return SYSCALL_FAULT_(EOPNOTSUPP);

    if (backlog <= 0)
        backlog = 5;
    if (backlog > 128)
        backlog = 128;

    spin_lock(info->lock);
    info->backlog       = backlog;
    info->pending_queue = calloc(backlog, sizeof(socket_info_t *));
    info->pending_count = 0;
    info->state         = SS_LISTENING;
    spin_unlock(info->lock);
    return EOK;
}

syscall_(accept, int sockfd, struct sockaddr *addr, uint64_t *addrlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (info->state != SS_LISTENING)
        return SYSCALL_FAULT_(EINVAL);

    // Wait for a pending connection
    for (;;) {
        spin_lock(info->lock);
        if (info->pending_count > 0) {
            // Dequeue the first pending client
            socket_info_t *client_info = info->pending_queue[0];
            info->pending_count--;
            for (int i = 0; i < info->pending_count; i++)
                info->pending_queue[i] = info->pending_queue[i + 1];
            spin_unlock(info->lock);

            // Create server-side endpoint
            socket_info_t *server_info =
                alloc_socket_info(info->domain, info->type, info->protocol);
            if (!server_info)
                return SYSCALL_FAULT_(ENOMEM);

            // Cross-link
            server_info->peer  = client_info;
            server_info->state = SS_CONNECTED;

            spin_lock(client_info->lock);
            client_info->peer  = server_info;
            client_info->state = SS_CONNECTED;
            spin_unlock(client_info->lock);

            // Create fd for the new server endpoint
            int new_fd = sock_create_fd(server_info, 0);
            if (new_fd < 0)
                return SYSCALL_FAULT_(ENOMEM);

            // Fill in addr if requested
            if (addr && addrlen) {
                struct sockaddr_un *sun = (struct sockaddr_un *)addr;
                memset(sun, 0, sizeof(struct sockaddr_un));
                sun->sun_family = AF_UNIX;
                if (client_info->is_bound)
                    strncpy(sun->sun_path, client_info->bound_path, UNIX_PATH_MAX - 1);
                *addrlen = sizeof(struct sockaddr_un);
            }
            return new_fd;
        }
        spin_unlock(info->lock);
        scheduler_yield();
    }
}

syscall_(connect, int sockfd, struct sockaddr *addr, uint64_t addrlen) {
    if (!addr)
        return SYSCALL_FAULT_(EINVAL);
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);

    if (addr->sa_family != AF_UNIX)
        return SYSCALL_FAULT_(EAFNOSUPPORT);
    struct sockaddr_un *sun = (struct sockaddr_un *)addr;

    if (info->type == SOCK_DGRAM) {
        // For DGRAM, connect just sets the default peer
        char *path = vfs_cwd_path_build(sun->sun_path);
        if (!path)
            return SYSCALL_FAULT_(ENOMEM);
        vfs_node_t target_node = vfs_open(path);
        free(path);
        if (!target_node || !(target_node->type & file_socket))
            return SYSCALL_FAULT_(ECONNREFUSED);
        socket_specific_t *target_spec = (socket_specific_t *)target_node->handle;
        if (!target_spec)
            return SYSCALL_FAULT_(ECONNREFUSED);

        spin_lock(info->lock);
        info->peer  = target_spec->info;
        info->state = SS_CONNECTED;
        spin_unlock(info->lock);
        return EOK;
    }

    // SOCK_STREAM connect
    if (info->state == SS_CONNECTED)
        return SYSCALL_FAULT_(EISCONN);
    if (info->state == SS_CONNECTING)
        return SYSCALL_FAULT_(EALREADY);

    char *path = vfs_cwd_path_build(sun->sun_path);
    if (!path)
        return SYSCALL_FAULT_(ENOMEM);
    vfs_node_t target_node = vfs_open(path);
    free(path);
    if (!target_node || !(target_node->type & file_socket))
        return SYSCALL_FAULT_(ECONNREFUSED);

    socket_specific_t *server_spec = (socket_specific_t *)target_node->handle;
    if (!server_spec)
        return SYSCALL_FAULT_(ECONNREFUSED);
    socket_info_t *server_info = server_spec->info;
    if (!server_info || server_info->state != SS_LISTENING)
        return SYSCALL_FAULT_(ECONNREFUSED);

    // Enqueue into server's pending queue
    spin_lock(server_info->lock);
    if (server_info->pending_count >= server_info->backlog) {
        spin_unlock(server_info->lock);
        return SYSCALL_FAULT_(ECONNREFUSED);
    }
    server_info->pending_queue[server_info->pending_count++] = info;
    spin_unlock(server_info->lock);

    info->state = SS_CONNECTING;

    // Wait for accept to complete the connection
    for (;;) {
        spin_lock(info->lock);
        if (info->state == SS_CONNECTED) {
            spin_unlock(info->lock);
            return EOK;
        }
        spin_unlock(info->lock);
        scheduler_yield();
    }
}

syscall_(shutdown, int sockfd, int how) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (how == SHUT_RD || how == SHUT_RDWR)
        spec->shut_rd = true;
    if (how == SHUT_WR || how == SHUT_RDWR)
        spec->shut_wr = true;
    return EOK;
}

syscall_(
    sendto,
    int sockfd,
    void *buf,
    size_t len,
    int flags,
    struct sockaddr *dest_addr,
    uint64_t addrlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (spec->shut_wr)
        return SYSCALL_FAULT_(EPIPE);

    socket_info_t *target = info->peer;

    // For DGRAM with dest_addr, find the target
    if (info->type == SOCK_DGRAM && dest_addr) {
        struct sockaddr_un *sun = (struct sockaddr_un *)dest_addr;
        if (sun->sun_family == AF_UNIX && sun->sun_path[0] != '\0') {
            char *path = vfs_cwd_path_build(sun->sun_path);
            if (path) {
                vfs_node_t tnode = vfs_open(path);
                free(path);
                if (tnode && (tnode->type & file_socket)) {
                    socket_specific_t *ts = (socket_specific_t *)tnode->handle;
                    if (ts)
                        target = ts->info;
                }
            }
        }
    }

    if (!target)
        return SYSCALL_FAULT_(ENOTCONN);

    // Write data to target's recv_buf
    const uint8_t *src = (const uint8_t *)buf;
    size_t total       = 0;
    size_t remaining   = len;

    while (remaining > 0) {
        spin_lock(target->lock);
        size_t avail = target->recv_buf.capacity - target->recv_buf.count;
        if (avail > 0) {
            size_t to_write = MIN(remaining, avail);
            ringbuf_write(&target->recv_buf, src + total, to_write);
            total += to_write;
            remaining -= to_write;
            spin_unlock(target->lock);
            if (info->type == SOCK_DGRAM)
                break; // DGRAM: single write
        } else {
            spin_unlock(target->lock);
            if (flags & MSG_DONTWAIT)
                break;
            scheduler_yield();
        }
    }
    return total > 0 ? total : SYSCALL_FAULT_(EAGAIN);
}

syscall_(
    recvfrom,
    int sockfd,
    void *buf,
    size_t len,
    int flags,
    struct sockaddr *src_addr,
    uint64_t *addrlen
) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (spec->shut_rd)
        return 0;

    for (;;) {
        spin_lock(info->lock);
        if (info->recv_buf.count > 0) {
            size_t to_read = MIN(len, info->recv_buf.count);
            if (flags & MSG_PEEK)
                ringbuf_peek(&info->recv_buf, buf, to_read);
            else
                ringbuf_read(&info->recv_buf, buf, to_read);
            spin_unlock(info->lock);

            if (src_addr && addrlen) {
                struct sockaddr_un *sun = (struct sockaddr_un *)src_addr;
                memset(sun, 0, sizeof(struct sockaddr_un));
                sun->sun_family = AF_UNIX;
                *addrlen        = sizeof(struct sockaddr_un);
            }
            return to_read;
        }
        // Check for EOF (stream peer gone)
        if (info->type == SOCK_STREAM && info->peer == NULL && info->state == SS_CONNECTED) {
            spin_unlock(info->lock);
            return 0;
        }
        spin_unlock(info->lock);
        if (flags & MSG_DONTWAIT)
            return SYSCALL_FAULT_(EAGAIN);
        scheduler_yield();
    }
}

syscall_(sendmsg, int sockfd, struct msghdr *msg, int flags) {
    if (!msg)
        return SYSCALL_FAULT_(EINVAL);
    size_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0)
            continue;
        size_t ret = syscall_sendto(
            sockfd, iov->iov_base, iov->iov_len, flags, msg->msg_name, msg->msg_namelen, regs
        );
        if ((int64_t)ret < 0)
            return total > 0 ? total : ret;
        total += ret;
    }
    return total;
}

syscall_(recvmsg, int sockfd, struct msghdr *msg, int flags) {
    if (!msg)
        return SYSCALL_FAULT_(EINVAL);
    size_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0)
            continue;
        size_t ret = syscall_recvfrom(sockfd, iov->iov_base, iov->iov_len, flags, NULL, NULL, regs);
        if ((int64_t)ret < 0)
            return total > 0 ? total : ret;
        total += ret;
        if (ret < iov->iov_len)
            break; // short read
    }
    msg->msg_controllen = 0;
    msg->msg_flags      = 0;
    return total;
}

syscall_(setsockopt, int sockfd, int level, int optname, void *optval, uint64_t optlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    // Minimal implementation: accept common options silently
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return EOK;
}

syscall_(getsockopt, int sockfd, int level, int optname, void *optval, uint64_t *optlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (!optval || !optlen)
        return SYSCALL_FAULT_(EINVAL);

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
        case SO_RCVBUF:
            val = (int)info->recv_buf.capacity;
            break;
        case SO_SNDBUF:
            val = SOCK_BUFF;
            break;
        default:
            return EOK;
        }
        size_t copy_len = MIN(*optlen, sizeof(int));
        memcpy(optval, &val, copy_len);
        *optlen = sizeof(int);
        return EOK;
    }
    return EOK;
}

syscall_(getsockname, int sockfd, struct sockaddr *addr, uint64_t *addrlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (!addr || !addrlen)
        return SYSCALL_FAULT_(EINVAL);

    struct sockaddr_un *sun = (struct sockaddr_un *)addr;
    memset(sun, 0, sizeof(struct sockaddr_un));
    sun->sun_family = AF_UNIX;
    if (info->is_bound)
        strncpy(sun->sun_path, info->bound_path, UNIX_PATH_MAX - 1);
    *addrlen = sizeof(struct sockaddr_un);
    return EOK;
}

syscall_(getpeername, int sockfd, struct sockaddr *addr, uint64_t *addrlen) {
    socket_specific_t *spec = get_sock_spec(sockfd);
    if (!spec)
        return SYSCALL_FAULT_(ENOTSOCK);
    socket_info_t *info = spec->info;
    if (!info)
        return SYSCALL_FAULT_(ENOTSOCK);
    if (!addr || !addrlen)
        return SYSCALL_FAULT_(EINVAL);
    if (!info->peer)
        return SYSCALL_FAULT_(ENOTCONN);

    struct sockaddr_un *sun = (struct sockaddr_un *)addr;
    memset(sun, 0, sizeof(struct sockaddr_un));
    sun->sun_family = AF_UNIX;
    if (info->peer->is_bound)
        strncpy(sun->sun_path, info->peer->bound_path, UNIX_PATH_MAX - 1);
    *addrlen = sizeof(struct sockaddr_un);
    return EOK;
}
