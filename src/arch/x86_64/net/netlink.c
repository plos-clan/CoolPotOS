#include "kprint.h"
#include "lock_queue.h"
#include "network.h"
#include "pcb.h"
#include "sprintf.h"

spin_t                       netlink_sockets_lock                 = SPIN_INIT;
static int                   netlink_socket_fsid                  = 0;
static struct netlink_sock  *netlink_sockets[MAX_NETLINK_SOCKETS] = {0};
static struct uevent_message uevent_queue[MAX_UEVENT_MESSAGES];
static size_t                uevent_queue_head = 0;
static size_t                uevent_queue_tail = 0;
static spin_t                uevent_queue_lock = SPIN_INIT;

static void netlink_deliver_queued_uevents(struct netlink_sock *sock) {
    spin_lock(uevent_queue_lock);

    struct uevent_message *msg = &uevent_queue[sock->uevent_message_pos++];

    spin_lock(sock->lock);
    if (sock->buffer_pos + msg->length <= NETLINK_BUFFER_SIZE) {
        memcpy(sock->buffer + sock->buffer_pos, msg->buffer, msg->length);
        sock->buffer_pos += msg->length;
    }
    spin_unlock(sock->lock);

    spin_unlock(uevent_queue_lock);
}

// Function to add uevent to queue
static void netlink_queue_uevent(const char *message, size_t length) {
    spin_lock(uevent_queue_lock);

    struct uevent_message *msg = &uevent_queue[uevent_queue_tail];
    if (length <= NETLINK_BUFFER_SIZE) {
        memcpy(msg->buffer, message, length);
        msg->length    = length;
        msg->timestamp = 0; // TODO: Add timestamp support

        uevent_queue_tail = (uevent_queue_tail + 1) % MAX_UEVENT_MESSAGES;

        // If queue is full, overwrite oldest message
        if (uevent_queue_tail == uevent_queue_head) {
            uevent_queue_head = (uevent_queue_head + 1) % MAX_UEVENT_MESSAGES;
        }
    }

    spin_unlock(uevent_queue_lock);
}

int netlink_bind(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen) {
    if (addrlen < sizeof(struct sockaddr_nl)) { return -EINVAL; }
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;

    socket_handle_t     *handle = fd_handle->node->handle;
    struct netlink_sock *sock   = handle->sock;

    struct sockaddr_nl *nl_addr = (struct sockaddr_nl *)addr;

    if (nl_addr->nl_family != AF_NETLINK) { return -EAFNOSUPPORT; }

    spin_lock(sock->lock);
    memcpy(sock->bind_addr, nl_addr, sizeof(struct sockaddr_nl));
    sock->portid = nl_addr->nl_pid;
    sock->groups = nl_addr->nl_groups;
    spin_unlock(sock->lock);

    return 0;
}

size_t netlink_getsockopt(uint64_t fd, int level, int optname, const void *optval,
                          socklen_t *optlen) {
    // TODO: Implement netlink socket options
    return 0;
}

size_t netlink_setsockopt(uint64_t fd, int level, int optname, const void *optval,
                          socklen_t optlen) {
    // TODO: Implement netlink socket options
    return 0;
}

size_t netlink_recvmsg(uint64_t fd, struct msghdr *msg, int flags) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t     *handle = fd_handle->node->handle;
    struct netlink_sock *nl_sk  = handle->sock;

    bool   noblock      = !!(flags & MSG_DONTWAIT) || !!(fd_handle->flags & O_NONBLOCK);
    size_t total_copied = 0;

    if ((nl_sk->uevent_message_pos == uevent_queue_tail && nl_sk->buffer_pos == 0) && noblock) {
        spin_unlock(nl_sk->lock);
        return -EAGAIN;
    }

    // Wait for data if non-blocking and no data available
    while ((nl_sk->uevent_message_pos == uevent_queue_tail && nl_sk->buffer_pos == 0) && !noblock) {
        spin_unlock(nl_sk->lock);
        scheduler_yield();
        spin_lock(nl_sk->lock);
    }

    netlink_deliver_queued_uevents(nl_sk);

    spin_lock(nl_sk->lock);

    // Copy data to user iovec
    size_t remaining = nl_sk->buffer_pos;
    char  *src       = nl_sk->buffer;

    for (int i = 0; i < msg->msg_iovlen && remaining > 0; i++) {
        struct iovec *curr    = &msg->msg_iov[i];
        size_t        to_copy = (remaining < curr->iov_len) ? remaining : curr->iov_len;

        if (to_copy > 0) {
            memcpy(curr->iov_base, src, to_copy);
            src          += to_copy;
            remaining    -= to_copy;
            total_copied += to_copy;
        }
    }

    // Move remaining data to beginning of buffer
    if (remaining > 0) { memmove(nl_sk->buffer, src, remaining); }
    nl_sk->buffer_pos = remaining;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
    if (cmsg) {
        cmsg->cmsg_len     = sizeof(struct ucred);
        cmsg->cmsg_level   = SOL_SOCKET;
        cmsg->cmsg_type    = SCM_CREDENTIALS;
        struct ucred *cred = (struct ucred *)CMSG_DATA(cmsg);
        cred->pid          = 0;
        cred->gid          = 1;
        cred->uid          = 0;
    }

    spin_unlock(nl_sk->lock);

    return total_copied;
}

size_t netlink_sendmsg(uint64_t fd, const struct msghdr *msg, int flags) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t     *handle = fd_handle->node->handle;
    struct netlink_sock *nl_sk  = handle->sock;

    size_t total_len = 0;

    // Calculate total message length
    for (int i = 0; i < msg->msg_iovlen; i++) {
        total_len += msg->msg_iov[i].iov_len;
    }

    if (total_len > NETLINK_BUFFER_SIZE) { return -EMSGSIZE; }

    // Copy data from user iovec to buffer
    char  *buffer = malloc(total_len);
    size_t copied = 0;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = &msg->msg_iov[i];
        memcpy(buffer + copied, curr->iov_base, curr->iov_len);
        copied += curr->iov_len;
    }

    // For now, just echo back the message for testing
    spin_lock(nl_sk->lock);

    if (nl_sk->buffer_pos + total_len > NETLINK_BUFFER_SIZE) {
        spin_unlock(nl_sk->lock);
        free(buffer);
        return -ENOBUFS;
    }

    // for (struct nlmsghdr *nlmsg = (struct nlmsghdr *)buffer; NLMSG_OK(nlmsg,
    // total_len); nlmsg = NLMSG_NEXT(nlmsg, total_len))
    // {
    //     serial_fprintk("Netlink message type: %d\n", nlmsg->nlmsg_type);
    // }

    spin_unlock(nl_sk->lock);

    free(buffer);
    return total_len;
}

int netlink_getsockname(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen) {
    fd_file_handle *fd_handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (fd_handle == NULL) return -EBADF;
    socket_handle_t     *handle = fd_handle->node->handle;
    struct netlink_sock *nl_sk  = handle->sock;

    memcpy(addr, nl_sk->bind_addr, sizeof(struct sockaddr_nl));
    *addrlen = sizeof(struct sockaddr_nl);

    return 0;
}

// Broadcast uevent to all listening netlink sockets
static void netlink_broadcast_uevent(const char *buf, int len) {
    // Add message to uevent queue first
    netlink_queue_uevent(buf, len);

    spin_lock(netlink_sockets_lock);

    for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
        if (netlink_sockets[i] == NULL) continue;

        struct netlink_sock *sock = netlink_sockets[i];
        spin_lock(sock->lock);

        // Check if socket is bound to uevent group
        if (sock->groups & 1) { // Group 1 for uevents
            if (sock->buffer_pos + len <= NETLINK_BUFFER_SIZE) {
                memcpy(sock->buffer + sock->buffer_pos, buf, len);
                sock->buffer_pos += len;
            }
        }

        spin_unlock(sock->lock);
    }

    spin_unlock(netlink_sockets_lock);
}

socket_op_t netlink_ops = {
    .bind        = netlink_bind,
    .getsockopt  = netlink_getsockopt,
    .setsockopt  = netlink_setsockopt,
    .getsockname = netlink_getsockname,
    .recvmsg     = netlink_recvmsg,
    .sendmsg     = netlink_sendmsg,
};

int netlink_socket(int domain, int type, int protocol) {
    if (domain != AF_NETLINK) { return -EAFNOSUPPORT; }

    char buf[128];
    sprintf(buf, "sock%d", sockfsfd_id++);

    struct netlink_sock *nl_sk = malloc(sizeof(struct netlink_sock));
    memset(nl_sk, 0, sizeof(struct netlink_sock));
    nl_sk->portid    = (uint32_t)get_current_task()->parent_group->pid;
    nl_sk->bind_addr = malloc(sizeof(struct sockaddr_nl));
    memset(nl_sk->bind_addr, 0, sizeof(struct sockaddr_nl));
    nl_sk->buffer             = malloc(NETLINK_BUFFER_SIZE);
    nl_sk->uevent_message_pos = 0;
    nl_sk->buffer_size        = NETLINK_BUFFER_SIZE;
    nl_sk->buffer_pos         = 0;
    nl_sk->lock               = SPIN_INIT;

    socket_handle_t *handle = malloc(sizeof(socket_handle_t));
    memset(handle, 0, sizeof(socket_handle_t));

    vfs_node_t socknode = vfs_node_alloc(sockfs_root, buf);
    socknode->type      = file_socket;
    socknode->fsid      = netlink_socket_fsid;
    socknode->refcount++;
    socknode->handle = handle;

    handle->op   = &netlink_ops;
    handle->sock = nl_sk;

    // Add to global socket array
    spin_lock(netlink_sockets_lock);
    for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
        if (netlink_sockets[i] == NULL) {
            netlink_sockets[i] = nl_sk;

            // Deliver queued uevents to new socket if it's for uevents
            if (protocol == NETLINK_KOBJECT_UEVENT) {
                nl_sk->groups = 1; // Automatically subscribe to uevent group
                // netlink_deliver_queued_uevents(nl_sk);
            }

            break;
        }
    }
    spin_unlock(netlink_sockets_lock);

    uint64_t flags = 0;
    if (type & O_NONBLOCK) { flags |= O_NONBLOCK; }
    if (type & O_CLOEXEC) { flags |= O_CLOEXEC; }

    fd_file_handle *fd_handle = malloc(sizeof(fd_file_handle));
    fd_handle->node           = socknode;
    fd_handle->offset         = 0;
    fd_handle->flags          = flags;
    fd_handle->fd = queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);

    return fd_handle->fd;
}

int netlink_socket_pair(int type, int protocol, int *sv) {
    // Netlink doesn't support socket pairs
    return -EOPNOTSUPP;
}

int netlink_poll(void *file, size_t events) {
    socket_handle_t     *handle = file;
    struct netlink_sock *nl_sk  = handle->sock;

    int revents = 0;
    if (events & EPOLLIN) {
        if (nl_sk->uevent_message_pos != uevent_queue_tail || nl_sk->buffer_pos > 0) {
            revents |= EPOLLIN;
        }
    }

    return revents;
}

size_t netlink_read(void *fd, void *addr, size_t offset, size_t size) {
    socket_handle_t     *handle = fd;
    struct netlink_sock *nl_sk  = handle->sock;

    bool   noblock      = false; //!!(fd->flags & O_NONBLOCK);
    size_t total_copied = 0;

    if ((nl_sk->uevent_message_pos == uevent_queue_tail && nl_sk->buffer_pos == 0) && noblock) {
        spin_unlock(nl_sk->lock);
        return -EAGAIN;
    }

    // Wait for data if non-blocking and no data available
    while ((nl_sk->uevent_message_pos == uevent_queue_tail && nl_sk->buffer_pos == 0) && !noblock) {
        spin_unlock(nl_sk->lock);
        scheduler_yield();
        spin_lock(nl_sk->lock);
    }

    netlink_deliver_queued_uevents(nl_sk);

    spin_lock(nl_sk->lock);

    // Copy data to user iovec
    size_t remaining = nl_sk->buffer_pos;
    char  *src       = nl_sk->buffer;

    size_t to_copy = (remaining < size) ? remaining : size;
    memcpy(addr, src, to_copy);

    // Move remaining data to beginning of buffer
    if (remaining > 0) { memmove(nl_sk->buffer, src, remaining); }
    nl_sk->buffer_pos = remaining;

    spin_unlock(nl_sk->lock);

    return total_copied;
}

size_t netlink_write(void *fd, const void *addr, size_t offset, size_t size) {
    return 0;
}

static int dummy() {
    return -1;
}

vfs_node_t netlink_dup(vfs_node_t src) {
    return src;
}

static struct vfs_callback netlink_callback = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)dummy,
    .close    = (vfs_close_t)dummy,
    .read     = netlink_read,
    .write    = netlink_write,
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
    .poll     = netlink_poll,
    .dup      = netlink_dup,
};

void netlink_init() {
    netlink_socket_fsid =
        vfs_regist("netlinksockfs", &netlink_callback, NETFS_REGISTER_ID, 0x534F434B);
    if (netlink_socket_fsid == VFS_STATUS_FAILED) {
        kerror("Cannot register netlink fs.");
        return;
    }
    // Initialize uevent queue
    spin_lock(uevent_queue_lock);
    uevent_queue_head = 0;
    uevent_queue_tail = 0;
    memset(uevent_queue, 0, sizeof(uevent_queue));
    spin_unlock(uevent_queue_lock);
}
