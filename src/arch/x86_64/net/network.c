#include "network.h"
#include "heap.h"
#include "klog.h"
#include "krlibc.h"
#include "lock_queue.h"
#include "pcb.h"
#include "sprintf.h"

netdev_t *netdevs[MAX_NETDEV_NUM] = {NULL};
socket_t  first_unix_socket;
socket_t  sockets[MAX_SOCKETS];

void regist_netdev(void *desc, uint8_t *mac, uint32_t mtu, netdev_send_t send, netdev_recv_t recv) {
    for (int i = 0; i < MAX_NETDEV_NUM; i++) {
        if (netdevs[i] == NULL) {
            netdevs[i]       = malloc(sizeof(netdev_t));
            netdevs[i]->desc = desc;
            netdevs[i]->mtu  = mtu;
            memcpy(netdevs[i]->mac, mac, sizeof(netdevs[i]->mac));
            netdevs[i]->send = send;
            netdevs[i]->recv = recv;
            break;
        }
    }
}

errno_t netdev_send(netdev_t *dev, void *data, uint32_t len) {
    errno_t ret = dev->send(dev->desc, data, len);
    return ret;
}

errno_t netdev_recv(netdev_t *dev, void *data, uint32_t len) {
    errno_t ret = dev->recv(dev->desc, data, len);
    return ret;
}

char *unix_socket_addr_safe(const struct sockaddr_un *addr, size_t len) {
    size_t addrLen = len - sizeof(addr->sun_family);
    if (addrLen <= 0) return (void *)-(EINVAL);

    bool abstract = (addr->sun_path[0] == '\0');
    int  skip     = abstract ? 1 : 0;

    char *safe = calloc(addrLen + 2, 1);
    if (!safe) return (void *)-(ENOMEM);

    memset(safe, 0, addrLen + 2);

    if (abstract && addr->sun_path[1] == '\0') {
        free(safe);
        return (char *)-EINVAL;
    }

    if (abstract) {
        safe[0] = '@';
        memcpy(safe + 1, addr->sun_path + skip, addrLen - skip);
    } else {
        memcpy(safe, addr->sun_path, addrLen);
    }

    return safe;
}

vfs_node_t unix_socket_accept_create(unix_socket_pair_t *dir) {
    char buf[128];
    sprintf(buf, "sock%d", sockfsfd_id++);
    vfs_node_t socknode = vfs_child_append(sockfs_root, buf, NULL);
    socknode->refcount++;
    socknode->type = file_socket;
    socknode->mode = 0700;

    socket_handle_t *handle = malloc(sizeof(socket_handle_t));
    memset(handle, 0, sizeof(socket_handle_t));
    handle->op   = &accept_ops;
    handle->sock = dir;

    socknode->fsid   = unix_accept_fsid;
    socknode->handle = handle;

    return socknode;
}

unix_socket_pair_t *unix_socket_allocate_pair() {
    unix_socket_pair_t *pair = malloc(sizeof(unix_socket_pair_t));
    memset(pair, 0, sizeof(unix_socket_pair_t));
    pair->clientBuffSize       = BUFFER_SIZE;
    pair->serverBuffSize       = BUFFER_SIZE;
    pair->serverBuff           = malloc(pair->serverBuffSize);
    pair->clientBuff           = malloc(pair->clientBuffSize);
    pair->client_pending_files = malloc(MAX_PENDING_FILES_COUNT * sizeof(fd_file_handle *));
    pair->server_pending_files = malloc(MAX_PENDING_FILES_COUNT * sizeof(fd_file_handle *));
    memset(pair->client_pending_files, 0, MAX_PENDING_FILES_COUNT * sizeof(fd_file_handle *));
    memset(pair->server_pending_files, 0, MAX_PENDING_FILES_COUNT * sizeof(fd_file_handle *));
    pair->pending_fds_size = MAX_PENDING_FILES_COUNT;
    return pair;
}

void unix_socket_free_pair(unix_socket_pair_t *pair) {
    free(pair->clientBuff);
    free(pair->serverBuff);
    if (pair->filename) free(pair->filename);
    free(pair->client_pending_files);
    free(pair->server_pending_files);
    free(pair);
}

void socket_accept_close(void *handle0) {
    socket_handle_t    *handle = handle0;
    unix_socket_pair_t *pair   = handle->sock;
    pair->serverFds--;

    if (pair->serverFds == 0 && pair->clientFds == 0) unix_socket_free_pair(pair);
}

void socket_socket_close(void *handle) {
    socket_handle_t *socket_handle = handle;
    socket_t        *unixSocket    = socket_handle->sock;
    if (unixSocket->timesOpened >= 1) { unixSocket->timesOpened--; }
    if (unixSocket->pair) {
        unixSocket->pair->clientFds--;
        if (!unixSocket->pair->clientFds && !unixSocket->pair->serverFds)
            unix_socket_free_pair(unixSocket->pair);
    }
    if (unixSocket->timesOpened == 0) {
        socket_t *browse = &first_unix_socket;

        while (browse && browse->next != unixSocket) {
            browse = browse->next;
        }

        browse->next = unixSocket->next;
        free(unixSocket);
        free(socket_handle);
    }
}

size_t unix_socket_accept_recv_from(uint64_t fd, uint8_t *out, size_t limit, int flags,
                                    struct sockaddr_un *addr, uint32_t *len) {
    UNUSED(addr, len);

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;
    while (true) {
        if (!pair->clientFds && pair->serverBuffPos == 0) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -EPIPE;
        } else if ((fd_handle->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
                   pair->serverBuffPos == 0) {
            return -(EWOULDBLOCK);
        } else if (pair->serverBuffPos > 0)
            break;
    }

    spin_lock(pair->lock);

    size_t toCopy = MIN(limit, pair->serverBuffPos);
    memcpy(out, pair->serverBuff, toCopy);
    memmove(pair->serverBuff, &pair->serverBuff[toCopy], pair->serverBuffPos - toCopy);
    pair->serverBuffPos -= toCopy;

    spin_unlock(pair->lock);

    return toCopy;
}

size_t unix_socket_accept_sendto(uint64_t fd, uint8_t *in, size_t limit, int flags,
                                 struct sockaddr_un *addr, uint32_t len) {
    // useless unless SOCK_DGRAM
    UNUSED(addr, len);

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;

    if (limit > pair->clientBuffSize) { limit = pair->clientBuffSize; }

    while (true) {
        if (!pair->clientFds) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -(EPIPE);
        }

        if ((pair->clientBuffPos + limit) <= pair->clientBuffSize) break;

        if (fd_handle->flags & O_NONBLOCK || flags & MSG_DONTWAIT) { return -(EWOULDBLOCK); }

        open_interrupt;

        __asm__ volatile("pause");
    }

    close_interrupt;

    spin_lock(pair->lock);

    limit = MIN(limit, pair->clientBuffSize);
    memcpy(&pair->clientBuff[pair->clientBuffPos], in, limit);
    pair->clientBuffPos += limit;

    spin_unlock(pair->lock);

    return limit;
}

errno_t socket_accept_poll(void *file, size_t events) {
    socket_handle_t    *handle  = file;
    unix_socket_pair_t *pair    = handle->sock;
    int                 revents = 0;

    if (!pair->clientFds) revents |= EPOLLHUP;

    if ((events & EPOLLOUT) && pair->clientFds && pair->clientBuffPos < pair->clientBuffSize)
        revents |= EPOLLOUT;
    else if (events & EPOLLOUT)
        logkf("network: socket is full!!!\n");

    if ((events & EPOLLIN) && pair->serverBuffPos > 0) revents |= EPOLLIN;

    return revents;
}

int socket_socket(int domain, int type, int protocol) {
    char buf[128];
    sprintf(buf, "sock%d", sockfsfd_id++);
    vfs_node_t socknode = vfs_node_alloc(sockfs_root, buf);
    socknode->type      = file_socket;
    socknode->fsid      = unix_socket_fsid;
    socknode->refcount++;
    socket_handle_t *handle = malloc(sizeof(socket_handle_t));
    memset(handle, 0, sizeof(socket_handle_t));
    socket_t *unix_socket = malloc(sizeof(socket_t));
    memset(unix_socket, 0, sizeof(socket_t));

    socket_t *head = &first_unix_socket;
    while (head->next) {
        head = head->next;
    }

    head->next = unix_socket;

    handle->sock     = unix_socket;
    handle->op       = &socket_ops;
    socknode->handle = handle;

    unix_socket->timesOpened = 1;

    fd_file_handle *fd_handle = malloc(sizeof(fd_file_handle));
    fd_handle->node           = socknode;
    fd_handle->offset         = 0;
    fd_handle->flags          = 0;
    fd_handle->fd = queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);

    handle->fd = fd_handle;

    return fd_handle->fd;
}

int socket_bind(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;
    socket_t        *sock   = handle->sock;

    if (sock->bindAddr) return -(EINVAL);

    char *safe = unix_socket_addr_safe(addr, addrlen);
    if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK) return (uint64_t)safe;

    bool is_abstract = (addr->sun_path[0] == '\0');

    size_t    safeLen = strlen(safe);
    socket_t *browse  = &first_unix_socket;
    while (browse) {
        if (browse != sock && browse->bindAddr && strlen(browse->bindAddr) == safeLen &&
            memcmp(safe, browse->bindAddr, safeLen) == 0)
            break;
        browse = browse->next;
    }

    if (browse) {
        free(safe);
        return -(EADDRINUSE);
    }

    if (!is_abstract) {
        vfs_node_t new_node = vfs_open(safe);
        if (new_node) {
            // free(safe);
            // return -(EADDRINUSE);
        } else {
            //TODO vfs_mknod(safe, S_IFSOCK | 0666, 0);
        }
    }

    sock->bindAddr = safe;
    return 0;
}

int socket_listen(uint64_t fd, int backlog) {
    if (backlog == 0) // newer kernel behavior
        backlog = 16;
    if (backlog < 0) backlog = 128;

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;
    socket_t        *sock   = handle->sock;

    // maybe do a typical array here
    sock->connMax = backlog;
    sock->backlog = calloc(sizeof(unix_socket_pair_t *), sock->connMax);
    return 0;
}

int socket_accept(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen, uint64_t flags) {
    if (addr && addrlen && *addrlen > 0) {}

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;
    socket_t        *sock   = handle->sock;

    while (true) {
        if (sock->connCurr > 0) break;
        if (fd_handle->flags & O_NONBLOCK) {
            sock->acceptWouldBlock = true;
            return -(EWOULDBLOCK);
        } else
            sock->acceptWouldBlock = false;

        open_interrupt;
        __asm__ volatile("pause");
    }

    close_interrupt;

    unix_socket_pair_t *pair = sock->backlog[0];
    pair->established        = true;
    pair->serverFds++;
    pair->filename = strdup(sock->bindAddr);

    vfs_node_t acceptFd = unix_socket_accept_create(pair);
    sock->backlog[0]    = NULL;
    memmove(sock->backlog, &sock->backlog[1], (sock->connCurr) * sizeof(unix_socket_pair_t *));
    sock->connCurr--;

    fd_file_handle *fdhandle0 = malloc(sizeof(fd_file_handle));
    fdhandle0->node           = acceptFd;
    fdhandle0->offset         = 0;
    fdhandle0->flags          = flags;
    fdhandle0->fd = queue_enqueue(get_current_task()->parent_group->file_open, fdhandle0);

    socket_handle_t *accept_handle = acceptFd->handle;
    accept_handle->fd              = fdhandle0;

    return fdhandle0->fd;
}

int socket_connect(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;
    socket_t        *sock   = handle->sock;

    if (sock->connMax != 0) // already ran listen()
        return -(ECONNREFUSED);

    if (sock->pair) // already ran connect()
        return -(EISCONN);

    char *safe = unix_socket_addr_safe(addr, addrlen);
    if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK) return (uint64_t)safe;
    size_t safeLen = strlen(safe);

    socket_t *parent = &first_unix_socket;
    while (parent) {
        if (parent == sock) {
            parent = parent->next;
            continue;
        }

        if (parent->bindAddr && strlen(parent->bindAddr) == safeLen &&
            memcmp(safe, parent->bindAddr, safeLen) == 0)
            break;

        parent = parent->next;
    }
    free(safe);

    if (!parent) return -(ENOENT);

    if (parent->acceptWouldBlock && fd_handle->flags & O_NONBLOCK) { return -(EINPROGRESS); }

    if (!parent->connMax) { return -(ECONNREFUSED); }

    if (parent->connCurr >= parent->connMax) {
        return -(ECONNREFUSED); // no slot
    }

    unix_socket_pair_t *pair          = unix_socket_allocate_pair();
    sock->pair                        = pair;
    pair->clientFds                   = 1;
    parent->backlog[parent->connCurr] = pair;

    pair->peercred.pid = get_current_task()->parent_group->pid;
    pair->peercred.uid = get_current_task()->parent_group->user->uid;
    pair->peercred.gid = get_current_task()->parent_group->pgid;
    pair->has_peercred = true;

    parent->connCurr++;

    while (!pair->established) {
        scheduler_yield();
    }

    return 0;
}

size_t unix_socket_recv_from(uint64_t fd, uint8_t *out, size_t limit, int flags,
                             struct sockaddr_un *addr, uint32_t *len) {
    // useless unless SOCK_DGRAM
    UNUSED(addr, len);

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    socket_t           *socket = handle->sock;
    unix_socket_pair_t *pair   = socket->pair;
    if (!pair) { return -(ENOTCONN); }
    if (!pair->serverFds && pair->clientBuffPos == 0) return 0;
    while (true) {
        if (!pair->serverFds && pair->clientBuffPos == 0) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -EPIPE;
        } else if ((fd_handle->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
                   pair->clientBuffPos == 0) {
            return -(EWOULDBLOCK);
        } else if (pair->clientBuffPos > 0)
            break;
    }

    spin_lock(pair->lock);

    size_t toCopy = MIN(limit, pair->clientBuffPos);
    memcpy(out, pair->clientBuff, toCopy);
    memmove(pair->clientBuff, &pair->clientBuff[toCopy], pair->clientBuffPos - toCopy);
    pair->clientBuffPos -= toCopy;

    spin_unlock(pair->lock);

    return toCopy;
}

size_t unix_socket_send_to(uint64_t fd, uint8_t *in, size_t limit, int flags,
                           struct sockaddr_un *addr, uint32_t len) {
    // useless unless SOCK_DGRAM
    UNUSED(addr, len);

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    socket_t           *socket = handle->sock;
    unix_socket_pair_t *pair   = socket->pair;
    if (!pair) { return -(ENOTCONN); }
    if (limit > pair->serverBuffSize) { limit = pair->serverBuffSize; }

    while (true) {
        if (!pair->serverFds) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -(EPIPE);
        } else if ((fd_handle->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
                   (pair->serverBuffPos + limit) > pair->serverBuffSize) {
            return -(EWOULDBLOCK);
        } else if ((pair->serverBuffPos + limit) <= pair->serverBuffSize)
            break;

        scheduler_yield();
    }

    spin_lock(pair->lock);

    limit = MIN(limit, pair->serverBuffSize);
    memcpy(&pair->serverBuff[pair->serverBuffPos], in, limit);
    pair->serverBuffPos += limit;

    spin_unlock(pair->lock);

    return limit;
}

size_t unix_socket_recv_msg(uint64_t fd, struct msghdr *msg, int flags) {
    size_t cnt     = 0;
    bool   noblock = !!(flags & MSG_DONTWAIT);

    fd_file_handle *fd_handle0 = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle0 == NULL) return -EBADF;

    while (!noblock && !(fd_handle0->flags & O_NONBLOCK) &&
           !(vfs_poll(fd_handle0->node, EPOLLIN) & EPOLLIN)) {
        scheduler_yield();
    }

    if (!(vfs_poll(fd_handle0->node, EPOLLIN) & EPOLLIN)) { return (size_t)-EWOULDBLOCK; }

    int iov_len_total = 0;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        iov_len_total += curr->iov_len;
    }

    char *buffer = malloc(iov_len_total);

    cnt = unix_socket_recv_from(fd, (uint8_t *)buffer, iov_len_total, noblock ? MSG_DONTWAIT : 0,
                                NULL, 0);
    if ((int64_t)cnt < 0) {
        free(buffer);
        return cnt;
    }

    char *b = buffer;

    uint64_t remain = cnt;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        memcpy(curr->iov_base, b, MIN(curr->iov_len, remain));

        b      += MIN(curr->iov_len, remain);
        remain -= MIN(curr->iov_len, remain);
    }

    free(buffer);

    if (msg->msg_control && msg->msg_controllen >= sizeof(struct cmsghdr)) {
        fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (fd_handle == NULL) return -EBADF;
        socket_handle_t    *handle = fd_handle->node->handle;
        socket_t           *sock   = handle->sock;
        unix_socket_pair_t *pair   = sock->pair;
        if (!pair) return (size_t)-ENOTCONN;

        size_t max_fds      = (msg->msg_controllen - sizeof(struct cmsghdr)) / sizeof(int);
        size_t received_fds = 0;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg) {
            msg->msg_controllen = 0;
            return cnt;
        }
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;

        int *fds = (int *)CMSG_DATA(cmsg);

        spin_lock(pair->lock);

        for (int i = 0; i < MAX_PENDING_FILES_COUNT; i++) {
            if (pair->client_pending_files[i] == NULL) continue;
            fd_file_handle *ped_handle = malloc(sizeof(fd_file_handle));
            ped_handle->fd = queue_enqueue(get_current_task()->parent_group->file_open, ped_handle);
            memcpy(ped_handle, pair->client_pending_files[i], sizeof(fd_file_handle));
            free(pair->client_pending_files[i]);
            pair->client_pending_files[i] = NULL;
            fds[received_fds]             = fd;

            if (received_fds + 1 >= max_fds) break;

            received_fds++;
        }

        if (received_fds > 0) {
            cmsg->cmsg_len      = CMSG_LEN(received_fds * sizeof(int));
            msg->msg_controllen = cmsg->cmsg_len;
        } else {
            msg->msg_controllen = 0;
        }

        spin_unlock(pair->lock);
    } else {
        msg->msg_controllen = 0;
    }

    return cnt;
}

size_t unix_socket_send_msg(uint64_t fd, const struct msghdr *msg, int flags) {
    size_t cnt     = 0;
    bool   noblock = !!(flags & MSG_DONTWAIT);

    if (msg->msg_control) {
        fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (fd_handle == NULL) return -EBADF;
        socket_handle_t    *handle = fd_handle->node->handle;
        socket_t           *socket = handle->sock;
        unix_socket_pair_t *pair   = socket->pair;
        if (!pair) return (size_t)-ENOTCONN;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg) { goto no_cmsg; }

        spin_lock(pair->lock);

        for (; cmsg != NULL; cmsg = CMSG_NXTHDR((struct msghdr *)msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                int *fds     = (int *)CMSG_DATA(cmsg);
                int  num_fds = (cmsg->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);

                for (int i = 0; i < num_fds; i++) {
                    for (int j = 0; j < MAX_PENDING_FILES_COUNT; j++) {
                        if (pair->server_pending_files[j] == NULL) {
                            pair->server_pending_files[j] = malloc(sizeof(fd_file_handle));
                            fd_file_handle *pend_handle =
                                queue_get(get_current_task()->parent_group->file_open, fds[i]);
                            not_null_assets(pend_handle, "network: pend_handle null");
                            memcpy(pair->server_pending_files[j], pend_handle,
                                   sizeof(fd_file_handle));
                            pend_handle->node->refcount++;
                            break;
                        }
                    }
                }
            }
        }

        spin_unlock(pair->lock);
    }

no_cmsg:
    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        size_t singleCnt = unix_socket_send_to(fd, curr->iov_base, curr->iov_len,
                                               noblock ? MSG_DONTWAIT : 0, NULL, 0);

        if ((int64_t)singleCnt < 0) return singleCnt;

        cnt += singleCnt;
    }
    return cnt;
}

size_t unix_socket_accept_recv_msg(uint64_t fd, struct msghdr *msg, int flags) {
    size_t cnt     = 0;
    bool   noblock = !!(flags & MSG_DONTWAIT);

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    while (!noblock && !(fd_handle->flags & O_NONBLOCK) &&
           !(vfs_poll(fd_handle->node, EPOLLIN) & EPOLLIN)) {
        scheduler_yield();
    }

    if (!(vfs_poll(fd_handle->node, EPOLLIN) & EPOLLIN)) { return (size_t)-EWOULDBLOCK; }

    int iov_len_total = 0;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        iov_len_total += curr->iov_len;
    }

    char *buffer = malloc(iov_len_total);

    cnt = unix_socket_accept_recv_from(fd, (uint8_t *)buffer, iov_len_total,
                                       noblock ? MSG_DONTWAIT : 0, NULL, 0);
    if ((int64_t)cnt < 0) {
        free(buffer);
        return cnt;
    }

    char *b = buffer;

    uint32_t remain = cnt;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        memcpy(curr->iov_base, b, MIN(curr->iov_len, remain));

        b      += MIN(curr->iov_len, remain);
        remain -= MIN(curr->iov_len, remain);
    }

    free(buffer);

    if (msg->msg_control && msg->msg_controllen >= sizeof(struct cmsghdr)) {
        fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (fd_handle == NULL) return -EBADF;
        socket_handle_t    *handle = fd_handle->node->handle;
        unix_socket_pair_t *pair   = handle->sock;

        size_t max_fds      = (msg->msg_controllen - sizeof(struct cmsghdr)) / sizeof(int);
        size_t received_fds = 0;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg) {
            msg->msg_controllen = 0;
            return cnt;
        }
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;

        int *fds = (int *)CMSG_DATA(cmsg);

        spin_lock(pair->lock);

        for (int i = 0; i < MAX_PENDING_FILES_COUNT; i++) {
            if (pair->server_pending_files[i] == NULL) continue;

            int fd = get_current_task()->parent_group->file_open->size;

            fd_file_handle *pair_handle = calloc(1, sizeof(fd_file_handle));
            pair_handle->fd =
                queue_enqueue(get_current_task()->parent_group->file_open, pair_handle);
            memcpy(pair_handle, pair->server_pending_files[i], sizeof(fd_file_handle));
            free(pair->server_pending_files[i]);
            pair->server_pending_files[i] = NULL;
            fds[received_fds]             = fd;

            if (received_fds + 1 >= max_fds) break;

            received_fds++;
        }

        if (received_fds > 0) {
            cmsg->cmsg_len      = CMSG_LEN(received_fds * sizeof(int));
            msg->msg_controllen = cmsg->cmsg_len;
        } else {
            msg->msg_controllen = 0;
        }

        spin_unlock(pair->lock);
    } else {
        msg->msg_controllen = 0;
    }

    return cnt;
}

size_t unix_socket_accept_send_msg(uint64_t fd, const struct msghdr *msg, int flags) {
    size_t cnt     = 0;
    bool   noblock = !!(flags & MSG_DONTWAIT);

    if (msg->msg_control) {
        fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (fd_handle == NULL) return -EBADF;
        socket_handle_t    *handle = fd_handle->node->handle;
        unix_socket_pair_t *pair   = handle->sock;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg) { goto no_cmsg; }

        spin_lock(pair->lock);

        for (; cmsg != NULL; cmsg = CMSG_NXTHDR((struct msghdr *)msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                int *fds     = (int *)CMSG_DATA(cmsg);
                int  num_fds = (cmsg->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);

                for (int i = 0; i < num_fds; i++) {
                    for (int j = 0; j < MAX_PENDING_FILES_COUNT; j++) {
                        if (pair->client_pending_files[j] == NULL) {
                            pair->client_pending_files[j] = malloc(sizeof(fd_file_handle));

                            fd_file_handle *fd_file_ =
                                queue_get(get_current_task()->parent_group->file_open, fds[i]);
                            if (fd_file_ == NULL) return -EMFILE;

                            memcpy(pair->client_pending_files[j], fd_file_, sizeof(fd_file_handle));
                            fd_file_->node->refcount++;
                            break;
                        }
                    }
                }
            }
        }

        spin_unlock(pair->lock);
    }

no_cmsg:
    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));

        size_t singleCnt = unix_socket_accept_sendto(fd, curr->iov_base, curr->iov_len,
                                                     noblock ? MSG_DONTWAIT : 0, NULL, 0);

        if ((int64_t)singleCnt < 0) return singleCnt;

        cnt += singleCnt;
    }
    return cnt;
}

errno_t unix_socket_pair(int type, int protocol, int *sv) {
    size_t sock1 = socket_socket(1, type, protocol);
    if ((int64_t)(sock1) < 0) return sock1;

    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sock1);
    if (fd_handle == NULL) return -EBADF;
    vfs_node_t sock1Fd = fd_handle->node;

    unix_socket_pair_t *pair = unix_socket_allocate_pair();
    pair->clientFds          = 1;
    pair->serverFds          = 1;

    socket_handle_t *handle = sock1Fd->handle;
    socket_t        *sock   = handle->sock;
    sock->pair              = pair;
    sock->connMax           = 0;
    handle->sock            = sock;

    vfs_node_t sock2Fd = unix_socket_accept_create(pair);

    pair->peercred.pid = get_current_task()->parent_group->pid;
    pair->peercred.uid = get_current_task()->parent_group->user->uid;
    pair->peercred.gid = get_current_task()->parent_group->pgid;
    pair->has_peercred = true;

    socket_handle_t *new_handle = sock2Fd->handle;
    UNUSED(new_handle);

    fd_handle = malloc(sizeof(fd_file_handle));

    fd_handle->node   = sock2Fd;
    fd_handle->offset = 0;
    fd_handle->flags  = 0;
    fd_handle->fd     = queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);

    // finish it off
    sv[0] = sock1;
    sv[1] = fd_handle->fd;

    return 0;
}

errno_t socket_socket_poll(void *file, size_t events) {
    socket_handle_t *handler = file;
    socket_t        *socket  = handler->sock;
    int              revents = 0;

    if (socket->connMax > 0) {
        // listen()
        if (socket->connCurr < socket->connMax) // can connect()
            revents |= (events & EPOLLOUT) ? EPOLLOUT : 0;
        if (socket->connCurr > 0) // can accept()
            revents |= (events & EPOLLIN) ? EPOLLIN : 0;
    } else if (socket->pair) {
        // connect()
        unix_socket_pair_t *pair = socket->pair;
        if (!pair->serverFds) revents |= EPOLLHUP;

        if ((events & EPOLLOUT) && pair->serverFds && pair->serverBuffPos < pair->serverBuffSize)
            revents |= EPOLLOUT;
        else if (events & EPOLLOUT)
            logkf("network: socket is full!!!\n");

        if ((events & EPOLLIN) && pair->clientBuffPos > 0) revents |= EPOLLIN;
    } else {
        revents |= EPOLLHUP;
    }

    return revents;
}

size_t unix_socket_setsockopt(uint64_t fd, int level, int optname, const void *optval,
                              socklen_t optlen) {
    if (level != SOL_SOCKET) { return -ENOPROTOOPT; }
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;

    socket_t           *sock = handle->sock;
    unix_socket_pair_t *pair = sock->pair;

    switch (optname) {
    case SO_REUSEADDR:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            pair->reuseaddr = *(int *)optval;
        else
            return (size_t)-ENOTCONN;
        break;
    case SO_KEEPALIVE:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            pair->keepalive = *(int *)optval;
        else
            return (size_t)-ENOTCONN;
        break;
    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(&pair->sndtimeo, optval, sizeof(struct timeval));
        break;
    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(&pair->rcvtimeo, optval, sizeof(struct timeval));
        break;
    case SO_BINDTODEVICE:
        if (optlen > IFNAMSIZ) { return -EINVAL; }
        if (pair) {
            strncpy(pair->bind_to_dev, optval, optlen);
            pair->bind_to_dev[IFNAMSIZ - 1] = '\0';
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_LINGER:
        if (optlen < sizeof(struct linger)) { return -EINVAL; }
        memcpy(&pair->linger_opt, optval, sizeof(struct linger));
        break;
    case SO_SNDBUF:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair) {
            int new_size = *(int *)optval;
            if (new_size < BUFFER_SIZE) { new_size = BUFFER_SIZE; }
            void *newBuff = malloc(new_size);
            memcpy(newBuff, pair->serverBuff, MIN(new_size, pair->serverBuffSize));
            free(pair->serverBuff);
            pair->serverBuff     = newBuff;
            pair->serverBuffSize = new_size;
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_RCVBUF:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair) {
            int new_size = *(int *)optval;
            if (new_size < BUFFER_SIZE) { new_size = BUFFER_SIZE; }
            void *newBuff = malloc(new_size);
            memcpy(newBuff, pair->clientBuff, MIN(new_size, pair->clientBuffSize));
            free(pair->clientBuff);
            pair->clientBuff     = newBuff;
            pair->clientBuffSize = new_size;
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_PASSCRED:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            pair->passcred = *(int *)optval;
        else
            sock->passcred = *(int *)optval;
        break;
    case SO_ATTACH_FILTER: {
        struct sock_fprog fprog;
        if (optlen < sizeof(fprog)) { return -EINVAL; }
        memcpy(&fprog, optval, sizeof(fprog));
        if (fprog.len > 64 || fprog.len == 0) { return -EINVAL; }

        // 分配内存保存过滤器
        pair->filter = malloc(sizeof(struct sock_filter) * fprog.len);
        memcpy(pair->filter, fprog.filter, sizeof(struct sock_filter) * fprog.len);
        pair->filter_len = fprog.len;
        break;
    }
    default: return -ENOPROTOOPT;
    }

    return 0;
}

size_t unix_socket_getsockopt(uint64_t fd, int level, int optname, const void *optval,
                              socklen_t *optlen) {
    if (level != SOL_SOCKET) { return -ENOPROTOOPT; }
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t *handle = fd_handle->node->handle;

    socket_t *sock = handle->sock;

    unix_socket_pair_t *pair = sock->pair;

    // 获取选项值
    switch (optname) {
    case SO_REUSEADDR:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->reuseaddr;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_KEEPALIVE:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->keepalive;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (*optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(optval, &pair->sndtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;
    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (*optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(optval, &pair->rcvtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;
    case SO_BINDTODEVICE:
        if (*optlen < IFNAMSIZ) { return -EINVAL; }
        if (pair) {
            strncpy(optval, pair->bind_to_dev, IFNAMSIZ);
            *optlen = strlen(pair->bind_to_dev);
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_PROTOCOL:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        *(int *)optval = sock->protocol;
        *optlen        = sizeof(int);
        break;
    case SO_LINGER:
        if (*optlen < sizeof(struct linger)) { return -EINVAL; }
        memcpy(optval, &pair->linger_opt, sizeof(struct linger));
        *optlen = sizeof(struct linger);
        break;
    case SO_SNDBUF:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->serverBuffSize;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_RCVBUF:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = sock->pair->clientBuffSize;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_PASSCRED:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->passcred;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_PEERCRED:
        if (!pair->has_peercred) { return -ENODATA; }
        if (*optlen < sizeof(struct ucred)) { return -EINVAL; }
        if (pair)
            memcpy(optval, &pair->peercred, sizeof(struct ucred));
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(struct ucred);
        break;
    case SO_ATTACH_FILTER:
        if (*optlen < sizeof(struct sock_fprog)) { return -EINVAL; }
        struct sock_fprog fprog = {.len = pair->filter_len, .filter = pair->filter};
        memcpy(optval, &fprog, sizeof(fprog));
        *optlen = sizeof(fprog);
        break;
    default: return -ENOPROTOOPT;
    }

    return 0;
}

size_t unix_socket_accept_setsockopt(uint64_t fd, int level, int optname, const void *optval,
                                     socklen_t optlen) {
    if (level != SOL_SOCKET) { return -ENOPROTOOPT; }
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;

    switch (optname) {
    case SO_REUSEADDR:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            pair->reuseaddr = *(int *)optval;
        else
            return (size_t)-ENOTCONN;
        break;
    case SO_KEEPALIVE:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            pair->keepalive = *(int *)optval;
        else
            return (size_t)-ENOTCONN;
        break;
    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(&pair->sndtimeo, optval, sizeof(struct timeval));
        break;
    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(&pair->rcvtimeo, optval, sizeof(struct timeval));
        break;
    case SO_BINDTODEVICE:
        if (optlen > IFNAMSIZ) { return -EINVAL; }
        if (pair) {
            strncpy(pair->bind_to_dev, optval, optlen);
            pair->bind_to_dev[IFNAMSIZ - 1] = '\0';
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_LINGER:
        if (optlen < sizeof(struct linger)) { return -EINVAL; }
        memcpy(&pair->linger_opt, optval, sizeof(struct linger));
        break;
    case SO_SNDBUF:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair) {
            int new_size = *(int *)optval;
            if (new_size < BUFFER_SIZE) { new_size = BUFFER_SIZE; }
            void *newBuff = malloc(new_size);
            memcpy(newBuff, pair->clientBuff, MIN(new_size, pair->clientBuffSize));
            free(pair->clientBuff);
            pair->clientBuff     = newBuff;
            pair->clientBuffSize = new_size;
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_RCVBUF:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair) {
            int new_size = *(int *)optval;
            if (new_size < BUFFER_SIZE) { new_size = BUFFER_SIZE; }
            void *newBuff = malloc(new_size);
            memcpy(newBuff, pair->serverBuff, MIN(new_size, pair->serverBuffSize));
            free(pair->serverBuff);
            pair->serverBuff     = newBuff;
            pair->serverBuffSize = new_size;
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_PASSCRED:
        if (optlen < sizeof(int)) { return -EINVAL; }
        if (pair) pair->passcred = *(int *)optval;
        break;
    case SO_ATTACH_FILTER: {
        struct sock_fprog fprog;
        if (optlen < sizeof(fprog)) { return -EINVAL; }
        memcpy(&fprog, optval, sizeof(fprog));
        if (fprog.len > 64 || fprog.len == 0) { return -EINVAL; }

        // 分配内存保存过滤器
        pair->filter = malloc(sizeof(struct sock_filter) * fprog.len);
        memcpy(pair->filter, fprog.filter, sizeof(struct sock_filter) * fprog.len);
        pair->filter_len = fprog.len;
        break;
    }
    default: return -ENOPROTOOPT;
    }

    return 0;
}

size_t unix_socket_accept_getsockopt(uint64_t fd, int level, int optname, const void *optval,
                                     socklen_t *optlen) {
    if (level != SOL_SOCKET) { return -ENOPROTOOPT; }
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;

    // 获取选项值
    switch (optname) {
    case SO_REUSEADDR:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->reuseaddr;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_KEEPALIVE:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->keepalive;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (*optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(optval, &pair->sndtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;
    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (*optlen < sizeof(struct timeval)) { return -EINVAL; }
        memcpy(optval, &pair->rcvtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;
    case SO_BINDTODEVICE:
        if (*optlen < IFNAMSIZ) { return -EINVAL; }
        if (pair) {
            strncpy(optval, pair->bind_to_dev, IFNAMSIZ);
            *optlen = strlen(pair->bind_to_dev);
        } else
            return (size_t)-ENOTCONN;
        break;
    case SO_LINGER:
        if (*optlen < sizeof(struct linger)) { return -EINVAL; }
        memcpy(optval, &pair->linger_opt, sizeof(struct linger));
        *optlen = sizeof(struct linger);
        break;
    case SO_SNDBUF:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->clientBuffSize;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_RCVBUF:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->serverBuffSize;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_PASSCRED:
        if (*optlen < sizeof(int)) { return -EINVAL; }
        if (pair)
            *(int *)optval = pair->passcred;
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(int);
        break;
    case SO_PEERCRED:
        if (!pair->has_peercred) { return -ENODATA; }
        if (*optlen < sizeof(struct ucred)) { return -EINVAL; }
        if (pair)
            memcpy(optval, &pair->peercred, sizeof(struct ucred));
        else
            return (size_t)-ENOTCONN;
        *optlen = sizeof(struct ucred);
        break;
    case SO_ATTACH_FILTER:
        if (*optlen < sizeof(struct sock_fprog)) { return -EINVAL; }
        struct sock_fprog fprog = {.len = pair->filter_len, .filter = pair->filter};
        memcpy(optval, &fprog, sizeof(fprog));
        *optlen = sizeof(fprog);
        break;
    default: return -ENOPROTOOPT;
    }

    return 0;
}

static int dummy() {
    return 0;
}

size_t unix_socket_getpeername(uint64_t fd, struct sockaddr_un *addr, socklen_t *len) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t    *handle = fd_handle->node->handle;
    socket_t           *socket = handle->sock;
    unix_socket_pair_t *pair   = socket->pair;
    if (!pair) return -(ENOTCONN);

    size_t actualLen = sizeof(addr->sun_family) + strlen(pair->filename);
    int    toCopy    = MIN(*len, actualLen);
    if (toCopy < sizeof(addr->sun_family)) return -(EINVAL);
    addr->sun_family = 1;
    if (pair->filename[0] == '@') {
        memcpy(addr->sun_path, pair->filename + 1, toCopy - sizeof(addr->sun_family) - 1);
        // addr->sun_path[0] = '\0';
        *len = toCopy - 1;
    } else {
        memcpy(addr->sun_path, pair->filename, toCopy - sizeof(addr->sun_family));
        *len = toCopy;
    }
    return 0;
}

size_t unix_socket_accept_getpeername(uint64_t fd, struct sockaddr_un *addr, socklen_t *len) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;

    size_t actualLen = sizeof(addr->sun_family) + strlen(pair->filename);
    int    toCopy    = MIN(*len, actualLen);
    if (toCopy < sizeof(addr->sun_family)) return -(EINVAL);
    addr->sun_family = 1;
    if (pair->filename[0] == '@') {
        memcpy(addr->sun_path, pair->filename + 1, toCopy - sizeof(addr->sun_family) - 1);
        // addr->sun_path[0] = '\0';
        *len = toCopy - 1;
    } else {
        memcpy(addr->sun_path, pair->filename, toCopy - sizeof(addr->sun_family));
        *len = toCopy;
    }
    return 0;
}

int unix_socket_getsockname(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t    *handle = fd_handle->node->handle;
    socket_t           *socket = handle->sock;
    unix_socket_pair_t *pair   = socket->pair;
    if (!pair) return -(ENOTCONN);

    addr->sun_family = 1;
    strcpy(addr->sun_path, pair->filename);
    *addrlen = sizeof(addr->sun_family) + strlen(pair->filename);

    return 0;
}

int unix_socket_accept_getsockname(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t    *handle = fd_handle->node->handle;
    unix_socket_pair_t *pair   = handle->sock;

    addr->sun_family = 1;
    strcpy(addr->sun_path, pair->filename);
    *addrlen = sizeof(addr->sun_family) + strlen(pair->filename);

    return 0;
}

socket_op_t socket_ops = {
    .accept      = socket_accept,
    .listen      = socket_listen,
    .getsockname = unix_socket_getsockname,
    .bind        = socket_bind,
    .connect     = socket_connect,
    .sendto      = unix_socket_send_to,
    .recvfrom    = unix_socket_recv_from,
    .sendmsg     = unix_socket_send_msg,
    .recvmsg     = unix_socket_recv_msg,
    .getpeername = unix_socket_getpeername,
    .getsockopt  = unix_socket_getsockopt,
    .setsockopt  = unix_socket_setsockopt,
};

socket_op_t accept_ops = {
    .getsockname = unix_socket_accept_getsockname,
    .sendto      = unix_socket_accept_sendto,
    .recvfrom    = unix_socket_accept_recv_from,
    .sendmsg     = unix_socket_accept_send_msg,
    .recvmsg     = unix_socket_accept_recv_msg,
    .getpeername = unix_socket_accept_getpeername,
    .getsockopt  = unix_socket_accept_getsockopt,
    .setsockopt  = unix_socket_accept_setsockopt,
};
