#include "lock_queue.h"
#include "network.h"
#include "pcb.h"

int sockfsfd_id = 0;

errno_t syscall_net_socket(int domain, int type, int protocol) {
    errno_t fd = -EAFNOSUPPORT;
    if (domain == 1)
        fd = socket_socket(domain, type, protocol);
    else if (domain == 16)
        fd = netlink_socket(domain, type, protocol);
    else
        for (int i = 0; i < socket_num; i++) {
            if (real_sockets[i]->domain == domain) {
                fd = real_sockets[i]->socket(domain, type & 0xff, protocol);
            }
        }

    if (!(fd < 0)) {
        fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (fd_handle == NULL) return -EBADF;
        if (type & O_CLOEXEC) fd_handle->flags |= O_CLOEXEC;
        if (type & O_NONBLOCK) fd_handle->flags |= O_NONBLOCK;
    }

    return fd;
}

uint64_t syscall_net_bind(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->bind) return handle->op->bind(sockfd, addr, addrlen);
    return 0;
}

uint64_t syscall_net_listen(int sockfd, int backlog) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->listen) return handle->op->listen(sockfd, backlog);
    return 0;
}

uint64_t syscall_net_accept(int sockfd, struct sockaddr_un *addr, socklen_t *addrlen,
                            uint64_t flags) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->accept) return handle->op->accept(sockfd, addr, addrlen, flags);
    return 0;
}

uint64_t syscall_net_connect(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->connect) return handle->op->connect(sockfd, addr, addrlen);
    return 0;
}

int64_t syscall_net_send(int sockfd, void *buff, size_t len, int flags,
                         struct sockaddr_un *dest_addr, socklen_t addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->sendto) return handle->op->sendto(sockfd, buff, len, flags, dest_addr, addrlen);
    return 0;
}

int64_t syscall_net_recv(int sockfd, void *buf, size_t len, int flags,
                         struct sockaddr_un *dest_addr, socklen_t *addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->recvfrom)
        return handle->op->recvfrom(sockfd, buf, len, flags, dest_addr, addrlen);
    return 0;
}

int64_t syscall_net_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->sendmsg) return handle->op->sendmsg(sockfd, msg, flags);
    return 0;
}

int64_t syscall_net_recvmsg(int sockfd, struct msghdr *msg, int flags) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, sockfd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t *handle = fd_handle->node->handle;
    if (handle->op->recvmsg) return handle->op->recvmsg(sockfd, msg, flags);
    return 0;
}
