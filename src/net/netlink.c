#include "driver/ioctl.h"
#include "errno.h"
#include "fs/fds.h"
#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/alloc.h"
#include "net/netlink.h"
#include "net/real_socket.h"
#include "net/rtnl.h"
#include "net/socket_filter.h"
#include "syscall.h"
#include "task/task.h"

typedef pcb_t task_t;

#define current_task (netlink_current_process())
#define fd_info fdts
#define fd_get_flags(fd) ((fd)->flags)
#define with_fd_info_lock(fdinfo, block) do block while (0)
#define procfs_on_open_file(task, fd) ((void)0)
#define MAX_FD_NUM ((int)((current_task && current_task->fd_info) ? current_task->fd_info->fds_length : 0))

static int netlink_socket_fsid = 0;
static vfs_node_t netlink_root = NULL;
static int netlinkfd_id = 0;

#define MAX_NETLINK_SOCKETS 256
static struct netlink_sock *netlink_sockets[MAX_NETLINK_SOCKETS] = {0};
static spin_t netlink_sockets_lock = SPIN_INIT;

#define MAX_NETLINK_MSG_POOL_SIZE 1024

struct netlink_msg_pool_entry {
    char message[NETLINK_BUFFER_SIZE];
    size_t length;
    uint64_t timestamp;
    uint32_t seqnum;
    char devpath[256];
    uint32_t nl_pid;
    uint32_t nl_groups;
    int protocol;
    bool valid;
};

static struct netlink_msg_pool_entry
    netlink_msg_pool[MAX_NETLINK_MSG_POOL_SIZE];
static uint32_t netlink_msg_pool_next = 0;
static spin_t netlink_msg_pool_lock = SPIN_INIT;

static inline task_t netlink_current_process(void) {
    tcb_t task = get_current_task();
    return task ? task->process : NULL;
}

static inline fd_t *netlink_fd_create(vfs_node_t node, uint64_t flags,
                                      bool cloexec) {
    fd_t *fd = calloc(1, sizeof(fd_t));
    if (!fd) {
        return NULL;
    }
    fd->node = node;
    fd->flags = flags;
    if (cloexec) {
        fd->flags |= O_CLOEXEC;
    }
    fd->fd = -1;
    return fd;
}

static inline socket_t *netlink_refinfo_create(void) {
    socket_t *refinfo = calloc(1, sizeof(socket_t));
    if (!refinfo) {
        return NULL;
    }
    refinfo->lock = SPIN_INIT;
    refinfo->refcount = 1;
    return refinfo;
}

static inline struct netlink_sock *netlink_handle_sock(socket_handle_t *handle) {
    if (!handle) {
        return NULL;
    }
    return (struct netlink_sock *)handle->sock;
}

static void netlink_msg_pool_add(const char *message, size_t length,
                                 uint32_t nl_pid, uint32_t nl_groups,
                                 int protocol, uint32_t seqnum,
                                 const char *devpath) {
    if (message == NULL || length == 0 || nl_groups == 0) {
        return;
    }

    spin_lock(netlink_msg_pool_lock);

    struct netlink_msg_pool_entry *entry =
        &netlink_msg_pool[netlink_msg_pool_next];
    entry->valid = true;
    entry->length =
        (length < NETLINK_BUFFER_SIZE) ? length : NETLINK_BUFFER_SIZE - 1;
    memcpy(entry->message, message, entry->length);
    entry->message[entry->length] = '\0';
    entry->timestamp = 0;
    entry->nl_pid = nl_pid;
    entry->nl_groups = nl_groups;
    entry->protocol = protocol;
    entry->seqnum = seqnum;

    if (devpath) {
        strncpy(entry->devpath, devpath, sizeof(entry->devpath) - 1);
        entry->devpath[sizeof(entry->devpath) - 1] = '\0';
    } else {
        entry->devpath[0] = '\0';
    }

    netlink_msg_pool_next =
        (netlink_msg_pool_next + 1) % MAX_NETLINK_MSG_POOL_SIZE;

    spin_unlock(netlink_msg_pool_lock);
}

static bool netlink_msg_pool_get_by_devpath(const char *devpath, char *buffer,
                                            size_t *length, uint32_t *nl_pid,
                                            uint32_t *nl_groups) {
    spin_lock(netlink_msg_pool_lock);
    bool found = false;

    for (int i = 0; i < MAX_NETLINK_MSG_POOL_SIZE; i++) {
        struct netlink_msg_pool_entry *entry = &netlink_msg_pool[i];
        if (entry->valid && strcmp(entry->devpath, devpath) == 0) {
            size_t copy_len =
                (entry->length < *length) ? entry->length : *length;
            memcpy(buffer, entry->message, copy_len);
            *length = copy_len;
            if (nl_pid) {
                *nl_pid = entry->nl_pid;
            }
            if (nl_groups) {
                *nl_groups = entry->nl_groups;
            }
            found = true;
            break;
        }
    }

    spin_unlock(netlink_msg_pool_lock);
    return found;
}

static void netlink_buffer_init(struct netlink_buffer *buf) {
    memset(buf, 0, sizeof(struct netlink_buffer));
    buf->size = NETLINK_BUFFER_SIZE;
    buf->lock = SPIN_INIT;
}

static inline void netlink_notify_sock(struct netlink_sock *sock,
                                       uint32_t events) {
    if (!sock || !sock->node || !events) {
        return;
    }
    vfs_poll_notify(sock->node, events);
}

static int netlink_wait_sock(struct netlink_sock *sock, uint32_t events,
                             const char *reason) {
    if (!sock || !sock->node || !get_current_task()) {
        return -EINVAL;
    }

    uint32_t want = events | EPOLLERR | EPOLLHUP | EPOLLNVAL | EPOLLRDHUP;
    if ((vfs_poll(sock->node, want) & want) != 0) {
        return EOK;
    }

    vfs_poll_wait_t wait;
    vfs_poll_wait_init(&wait, get_current_task(), want);
    if (vfs_poll_wait_arm(sock->node, &wait) < 0) {
        return -EINVAL;
    }
    int ret = vfs_poll_wait_sleep(sock->node, &wait, -1, reason);
    vfs_poll_wait_disarm(&wait);
    return ret;
}

size_t netlink_buffer_write_packet(struct netlink_sock *sock, const char *data,
                                   size_t len, uint32_t nl_pid,
                                   uint32_t nl_groups) {
    struct netlink_buffer *buf = sock ? sock->buffer : NULL;

    if (buf == NULL || data == NULL || len == 0) {
        return 0;
    }

    if (sock->filter) {
        uint32_t accept_bytes = bpf_run(sock->filter->filter, sock->filter->len,
                                        (const uint8_t *)data, (uint32_t)len);
        if (!accept_bytes) {
            return 0;
        }
        len = MIN(len, accept_bytes);
    }

    spin_lock(buf->lock);

    size_t used;
    if (buf->tail >= buf->head) {
        used = buf->tail - buf->head;
    } else {
        used = buf->size - buf->head + buf->tail;
    }
    size_t available_space = buf->size - used - 1;

    size_t total_needed = sizeof(struct netlink_packet_hdr) + len;
    if (available_space < total_needed) {
        spin_unlock(buf->lock);
        return 0;
    }

    struct netlink_packet_hdr hdr;
    hdr.nl_pid = nl_pid;
    hdr.nl_groups = nl_groups;
    hdr.length = (uint32_t)len;

    char *hdr_bytes = (char *)&hdr;
    for (size_t i = 0; i < sizeof(struct netlink_packet_hdr); i++) {
        buf->data[buf->tail] = hdr_bytes[i];
        buf->tail = (buf->tail + 1) % buf->size;
    }

    for (size_t i = 0; i < len; i++) {
        buf->data[buf->tail] = data[i];
        buf->tail = (buf->tail + 1) % buf->size;
    }

    spin_unlock(buf->lock);
    netlink_notify_sock(sock, EPOLLIN);
    return len;
}

static size_t netlink_buffer_read_packet(struct netlink_sock *sock, char *out,
                                         size_t out_len, uint32_t *nl_pid,
                                         uint32_t *nl_groups, bool peek) {
    struct netlink_buffer *buf = sock ? sock->buffer : NULL;

    if (buf == NULL) {
        return 0;
    }

    spin_lock(buf->lock);

    size_t available;
    if (buf->tail >= buf->head) {
        available = buf->tail - buf->head;
    } else {
        available = buf->size - buf->head + buf->tail;
    }

    if (available < sizeof(struct netlink_packet_hdr)) {
        spin_unlock(buf->lock);
        return 0;
    }

    struct netlink_packet_hdr hdr;
    char *hdr_bytes = (char *)&hdr;
    size_t pos = buf->head;
    for (size_t i = 0; i < sizeof(struct netlink_packet_hdr); i++) {
        hdr_bytes[i] = buf->data[pos];
        pos = (pos + 1) % buf->size;
    }

    if (available < sizeof(struct netlink_packet_hdr) + hdr.length) {
        spin_unlock(buf->lock);
        return 0;
    }

    if (nl_pid) {
        *nl_pid = hdr.nl_pid;
    }
    if (nl_groups) {
        *nl_groups = hdr.nl_groups;
    }

    size_t copy_len = 0;
    if (out != NULL && out_len > 0) {
        copy_len = (hdr.length < out_len) ? hdr.length : out_len;

        for (size_t i = 0; i < copy_len; i++) {
            out[i] = buf->data[pos];
            pos = (pos + 1) % buf->size;
        }

        for (size_t i = copy_len; i < hdr.length; i++) {
            pos = (pos + 1) % buf->size;
        }
    } else {
        for (size_t i = 0; i < hdr.length; i++) {
            pos = (pos + 1) % buf->size;
        }
        copy_len = hdr.length;
    }

    if (!peek) {
        buf->head = pos;
    }

    spin_unlock(buf->lock);
    return copy_len;
}

static bool netlink_buffer_has_msg(struct netlink_sock *sock) {
    struct netlink_buffer *buf = sock ? sock->buffer : NULL;

    if (buf == NULL) {
        return false;
    }

    spin_lock(buf->lock);

    size_t available;
    if (buf->tail >= buf->head) {
        available = buf->tail - buf->head;
    } else {
        available = buf->size - buf->head + buf->tail;
    }

    if (available < sizeof(struct netlink_packet_hdr)) {
        spin_unlock(buf->lock);
        return false;
    }

    struct netlink_packet_hdr hdr;
    char *hdr_bytes = (char *)&hdr;
    size_t pos = buf->head;
    for (size_t i = 0; i < sizeof(struct netlink_packet_hdr); i++) {
        hdr_bytes[i] = buf->data[pos];
        pos = (pos + 1) % buf->size;
    }

    bool has_complete_msg =
        (available >= sizeof(struct netlink_packet_hdr) + hdr.length);

    spin_unlock(buf->lock);
    return has_complete_msg;
}

static size_t netlink_buffer_peek_msg_len(struct netlink_sock *sock) {
    struct netlink_buffer *buf = sock ? sock->buffer : NULL;

    if (buf == NULL) {
        return 0;
    }

    spin_lock(buf->lock);

    size_t available;
    if (buf->tail >= buf->head) {
        available = buf->tail - buf->head;
    } else {
        available = buf->size - buf->head + buf->tail;
    }

    if (available < sizeof(struct netlink_packet_hdr)) {
        spin_unlock(buf->lock);
        return 0;
    }

    struct netlink_packet_hdr hdr;
    char *hdr_bytes = (char *)&hdr;
    size_t pos = buf->head;
    for (size_t i = 0; i < sizeof(struct netlink_packet_hdr); i++) {
        hdr_bytes[i] = buf->data[pos];
        pos = (pos + 1) % buf->size;
    }

    if (available < sizeof(struct netlink_packet_hdr) + hdr.length) {
        spin_unlock(buf->lock);
        return 0;
    }

    spin_unlock(buf->lock);
    return hdr.length;
}

static size_t netlink_buffer_available(struct netlink_buffer *buf) {
    if (buf == NULL) {
        return 0;
    }

    spin_lock(buf->lock);
    size_t avail;
    if (buf->tail >= buf->head) {
        avail = buf->tail - buf->head;
    } else {
        avail = buf->size - buf->head + buf->tail;
    }
    spin_unlock(buf->lock);
    return avail;
}

static void netlink_deliver_historical_messages(struct netlink_sock *sock) {
    if (sock == NULL || sock->buffer == NULL || sock->groups == 0) {
        return;
    }

    spin_lock(netlink_msg_pool_lock);

    int start = (int)netlink_msg_pool_next;
    for (int count = 0; count < MAX_NETLINK_MSG_POOL_SIZE; count++) {
        int i = (start + count) % MAX_NETLINK_MSG_POOL_SIZE;
        struct netlink_msg_pool_entry *entry = &netlink_msg_pool[i];

        if (!entry->valid) {
            continue;
        }
        if (entry->protocol != sock->protocol) {
            continue;
        }
        if (entry->nl_groups != sock->groups) {
            continue;
        }

        size_t written =
            netlink_buffer_write_packet(sock, entry->message, entry->length,
                                        entry->nl_pid, entry->nl_groups);
        if (written == 0) {
            break;
        }
    }

    spin_unlock(netlink_msg_pool_lock);
}

static size_t netlink_deliver_to_socket(struct netlink_sock *target,
                                        const char *data, size_t len,
                                        uint32_t sender_pid,
                                        uint32_t sender_groups) {
    if (target == NULL || target->buffer == NULL) {
        return 0;
    }

    return netlink_buffer_write_packet(target, data, len, sender_pid,
                                       sender_groups);
}

void netlink_broadcast_to_group(const char *buf, size_t len,
                                uint32_t sender_pid, uint32_t target_groups,
                                int protocol, uint32_t seqnum,
                                const char *devpath) {
    if (buf == NULL || len == 0) {
        return;
    }

    netlink_msg_pool_add(buf, len, sender_pid, target_groups, protocol, seqnum,
                         devpath);

    spin_lock(netlink_sockets_lock);

    for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
        if (netlink_sockets[i] == NULL) {
            continue;
        }

        struct netlink_sock *sock = netlink_sockets[i];
        if (sock->protocol != protocol) {
            continue;
        }

        spin_lock(sock->lock);
        if (sock->groups == target_groups) {
            netlink_buffer_write_packet(sock, buf, len, sender_pid,
                                        target_groups);
        }
        spin_unlock(sock->lock);
    }

    spin_unlock(netlink_sockets_lock);
}

static int netlink_bind(uint64_t fd, const struct sockaddr_un *addr,
                        socklen_t addrlen) {
    UNUSED(addrlen);

    if (current_task->fd_info->fds[fd] == NULL ||
        current_task->fd_info->fds[fd]->node == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    if (handle == NULL || handle->sock == NULL) {
        return -EBADF;
    }

    struct netlink_sock *sock = netlink_handle_sock(handle);
    struct sockaddr_nl *nl_addr = (struct sockaddr_nl *)addr;

    if (nl_addr->nl_family != AF_NETLINK) {
        return -EAFNOSUPPORT;
    }

    spin_lock(sock->lock);

    sock->portid = nl_addr->nl_pid;
    sock->groups = nl_addr->nl_groups;

    if (sock->bind_addr == NULL) {
        sock->bind_addr = malloc(sizeof(struct sockaddr_nl));
        if (sock->bind_addr == NULL) {
            spin_unlock(sock->lock);
            return -ENOMEM;
        }
    }
    memcpy(sock->bind_addr, nl_addr, sizeof(struct sockaddr_nl));
    sock->bind_addr->nl_pid = sock->portid;

    netlink_deliver_historical_messages(sock);

    spin_unlock(sock->lock);
    return 0;
}

static size_t netlink_getsockopt(uint64_t fd, int level, int optname,
                                 void *optval, socklen_t *optlen) {
    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_TYPE:
            *(int *)optval = nl_sk->type;
            *optlen = sizeof(int);
            break;
        case SO_PROTOCOL:
            *(int *)optval = nl_sk->protocol;
            *optlen = sizeof(int);
            break;
        case SO_REUSEADDR:
        case SO_PASSCRED:
            break;
        default:
            return -ENOPROTOOPT;
        }
    } else if (level == SOL_NETLINK) {
        return 0;
    } else {
        return -ENOPROTOOPT;
    }

    return 0;
}

static size_t netlink_setsockopt(uint64_t fd, int level, int optname,
                                 const void *optval, socklen_t optlen) {
    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_ATTACH_FILTER: {
            if (optlen < sizeof(struct sock_fprog)) {
                return (size_t)-EINVAL;
            }
            const struct sock_fprog *fprog = optval;
            if (!fprog->len) {
                return (size_t)-EINVAL;
            }

            struct sock_fprog *new_prog = malloc(sizeof(struct sock_fprog));
            if (!new_prog) {
                return (size_t)-ENOMEM;
            }

            new_prog->len = fprog->len;
            new_prog->filter =
                malloc(new_prog->len * sizeof(struct sock_filter));
            if (!new_prog->filter) {
                free(new_prog);
                return (size_t)-ENOMEM;
            }
            memset(new_prog->filter, 0,
                   new_prog->len * sizeof(struct sock_filter));
            if (!copy_from_user(new_prog->filter, fprog->filter,
                                new_prog->len * sizeof(struct sock_filter))) {
                free(new_prog->filter);
                free(new_prog);
                return (size_t)-EFAULT;
            }

            if (nl_sk->filter) {
                free(nl_sk->filter->filter);
                free(nl_sk->filter);
            }
            nl_sk->filter = new_prog;
            break;
        }
        case SO_DETACH_FILTER:
            if (nl_sk->filter) {
                free(nl_sk->filter->filter);
                free(nl_sk->filter);
                nl_sk->filter = NULL;
            }
            break;
        case SO_REUSEADDR:
        case SO_PASSCRED:
            break;
        default:
            return -ENOPROTOOPT;
        }
    } else if (level == SOL_NETLINK) {
        return 0;
    } else {
        return -ENOPROTOOPT;
    }

    return 0;
}

static int netlink_getsockname(uint64_t fd, struct sockaddr_un *addr,
                               socklen_t *addrlen) {
    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    spin_lock(nl_sk->lock);

    struct sockaddr_nl *nl_addr = (struct sockaddr_nl *)addr;
    nl_addr->nl_family = AF_NETLINK;
    nl_addr->nl_pid = nl_sk->portid;
    nl_addr->nl_groups = nl_sk->groups;
    nl_addr->nl_pad = 0;

    *addrlen = sizeof(struct sockaddr_nl);

    spin_unlock(nl_sk->lock);
    return 0;
}

static size_t netlink_recvmsg(uint64_t fd, struct msghdr *msg, int flags) {
    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    if (nl_sk->buffer == NULL) {
        return -EINVAL;
    }

    bool noblock =
        !!(flags & MSG_DONTWAIT) ||
        !!(fd_get_flags(current_task->fd_info->fds[fd]) & O_NONBLOCK);

    bool has_msg = netlink_buffer_has_msg(nl_sk);
    if (!has_msg && noblock) {
        return -EAGAIN;
    }

    while (!has_msg) {
        int reason = netlink_wait_sock(nl_sk, EPOLLIN, "netlink_recvmsg");
        if (reason != EOK) {
            return -EINTR;
        }
        has_msg = netlink_buffer_has_msg(nl_sk);
    }

    size_t total_user_len = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        total_user_len += msg->msg_iov[i].len;
    }

    char temp_buf[NETLINK_BUFFER_SIZE];
    uint32_t sender_pid = 0;
    uint32_t sender_groups = 0;

    size_t bytes_read = netlink_buffer_read_packet(
        nl_sk, temp_buf, sizeof(temp_buf), &sender_pid, &sender_groups, false);
    if (bytes_read == 0) {
        return -EAGAIN;
    }

    size_t total_copied = 0;
    size_t remaining = bytes_read;
    char *src = temp_buf;

    for (size_t i = 0; i < msg->msg_iovlen && remaining > 0; i++) {
        struct iovec *curr = &msg->msg_iov[i];
        size_t to_copy = (remaining < curr->len) ? remaining : curr->len;

        if (to_copy > 0) {
            memcpy(curr->iov_base, src, to_copy);
            src += to_copy;
            remaining -= to_copy;
            total_copied += to_copy;
        }
    }

    size_t ret_len = (flags & MSG_TRUNC) ? bytes_read : total_copied;

    if (msg->msg_control && msg->msg_controllen > 0) {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
        if (cmsg && msg->msg_controllen >= CMSG_LEN(sizeof(struct ucred))) {
            cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_CREDENTIALS;

            struct ucred *cred = (struct ucred *)CMSG_DATA(cmsg);
            cred->pid = (int32_t)sender_pid;
            cred->gid = 0;
            cred->uid = 0;

            msg->msg_controllen = cmsg->cmsg_len;
        } else {
            msg->msg_controllen = 0;
        }
    }

    if (msg->msg_name && msg->msg_namelen >= sizeof(struct sockaddr_nl)) {
        struct sockaddr_nl *nl_addr = (struct sockaddr_nl *)msg->msg_name;
        nl_addr->nl_family = AF_NETLINK;
        nl_addr->nl_pid = sender_pid;
        nl_addr->nl_groups = sender_groups;
        nl_addr->nl_pad = 0;
        msg->msg_namelen = sizeof(struct sockaddr_nl);
    }

    if (remaining > 0) {
        msg->msg_flags |= MSG_TRUNC;
    }

    UNUSED(total_user_len);
    return ret_len;
}

static size_t netlink_sendmsg(uint64_t fd, const struct msghdr *msg, int flags) {
    UNUSED(flags);

    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    size_t total_len = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        total_len += msg->msg_iov[i].len;
    }

    if (total_len == 0) {
        return 0;
    }
    if (total_len > NETLINK_BUFFER_SIZE - sizeof(struct netlink_packet_hdr)) {
        return -EMSGSIZE;
    }

    char buffer[NETLINK_BUFFER_SIZE];
    size_t offset = 0;

    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = &msg->msg_iov[i];
        if (curr->iov_base && curr->len > 0) {
            memcpy(buffer + offset, curr->iov_base, curr->len);
            offset += curr->len;
        }
    }

    uint32_t sender_pid = nl_sk->portid;
    uint32_t sender_groups = nl_sk->groups;

    struct sockaddr_nl *addr = (struct sockaddr_nl *)msg->msg_name;
    if (!addr) {
        return 0;
    }

    if (addr->nl_pid != 0) {
        spin_lock(netlink_sockets_lock);
        for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
            if (netlink_sockets[i] == NULL) {
                continue;
            }

            struct netlink_sock *sock = netlink_sockets[i];
            if (sock->portid == addr->nl_pid) {
                spin_lock(sock->lock);
                netlink_deliver_to_socket(sock, buffer, total_len, sender_pid,
                                          sender_groups);
                spin_unlock(sock->lock);
                break;
            }
        }
        spin_unlock(netlink_sockets_lock);
    } else if (addr->nl_groups != 0) {
        netlink_broadcast_to_group(buffer, total_len, sender_pid,
                                   addr->nl_groups, nl_sk->protocol, 0, NULL);
    } else {
        if (nl_sk->protocol == NETLINK_ROUTE) {
            rtnl_process_msg(nl_sk, buffer, total_len, sender_pid);
        }
        return total_len;
    }

    return total_len;
}

static size_t netlink_sendto(uint64_t fd, uint8_t *in, size_t limit, int flags,
                             struct sockaddr_un *addr, uint32_t len) {
    UNUSED(flags);
    UNUSED(len);

    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    if (limit == 0) {
        return 0;
    }
    if (limit > NETLINK_BUFFER_SIZE - sizeof(struct netlink_packet_hdr)) {
        return -EMSGSIZE;
    }

    uint32_t sender_pid = nl_sk->portid;
    uint32_t sender_groups = nl_sk->groups;
    struct sockaddr_nl *nl_addr = (struct sockaddr_nl *)addr;

    if (nl_addr == NULL) {
        return -EDESTADDRREQ;
    }
    if (nl_addr->nl_family != AF_NETLINK) {
        return -EAFNOSUPPORT;
    }

    if (nl_addr->nl_pid == 0 && nl_addr->nl_groups == 0) {
        if (nl_sk->protocol == NETLINK_ROUTE) {
            rtnl_process_msg(nl_sk, (char *)in, limit, sender_pid);
        }
        return limit;
    }

    if (nl_addr->nl_pid != 0) {
        spin_lock(netlink_sockets_lock);
        for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
            if (netlink_sockets[i] == NULL) {
                continue;
            }

            struct netlink_sock *sock = netlink_sockets[i];
            if (sock->portid == nl_addr->nl_pid) {
                spin_lock(sock->lock);
                netlink_deliver_to_socket(sock, (char *)in, limit, sender_pid,
                                          sender_groups);
                spin_unlock(sock->lock);
                break;
            }
        }
        spin_unlock(netlink_sockets_lock);
    } else if (nl_addr->nl_groups != 0) {
        netlink_broadcast_to_group((char *)in, limit, sender_pid,
                                   nl_addr->nl_groups, nl_sk->protocol, 0,
                                   NULL);
    }

    return limit;
}

static size_t netlink_recvfrom(uint64_t fd, uint8_t *out, size_t limit,
                               int flags, struct sockaddr_un *addr,
                               uint32_t *len) {
    if (current_task->fd_info->fds[fd] == NULL) {
        return -EBADF;
    }

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);

    if (nl_sk->buffer == NULL) {
        return -EINVAL;
    }

    bool noblock =
        !!(flags & MSG_DONTWAIT) ||
        !!(fd_get_flags(current_task->fd_info->fds[fd]) & O_NONBLOCK);
    bool peek = !!(flags & MSG_PEEK);

    size_t msg_len = netlink_buffer_peek_msg_len(nl_sk);
    if (peek && msg_len > 0) {
        return msg_len;
    }

    bool has_msg = netlink_buffer_has_msg(nl_sk);
    if (!has_msg && noblock) {
        return -EAGAIN;
    }

    while (!has_msg) {
        int reason = netlink_wait_sock(nl_sk, EPOLLIN, "netlink_recvfrom");
        if (reason != EOK) {
            return -EINTR;
        }
        has_msg = netlink_buffer_has_msg(nl_sk);
    }

    uint32_t sender_pid = 0;
    uint32_t sender_groups = 0;
    msg_len = netlink_buffer_peek_msg_len(nl_sk);

    char temp_buffer[NETLINK_BUFFER_SIZE];
    size_t bytes_read = netlink_buffer_read_packet(
        nl_sk, temp_buffer, limit, &sender_pid, &sender_groups, false);
    if (bytes_read == 0) {
        return -EAGAIN;
    }

    memcpy(out, temp_buffer, bytes_read);

    if (addr) {
        struct sockaddr_nl nl_addr_out;
        nl_addr_out.nl_family = AF_NETLINK;
        nl_addr_out.nl_pid = sender_pid;
        nl_addr_out.nl_groups = sender_groups;
        nl_addr_out.nl_pad = 0;
        memcpy(addr, &nl_addr_out, sizeof(struct sockaddr_nl));
    }

    if (len) {
        uint32_t addr_len = sizeof(struct sockaddr_nl);
        *len = addr_len;
    }

    return (flags & MSG_TRUNC) ? msg_len : bytes_read;
}

socket_op_t netlink_ops = {
    .bind = netlink_bind,
    .getsockopt = netlink_getsockopt,
    .setsockopt = netlink_setsockopt,
    .getsockname = netlink_getsockname,
    .sendto = netlink_sendto,
    .recvfrom = netlink_recvfrom,
    .recvmsg = netlink_recvmsg,
    .sendmsg = netlink_sendmsg,
};

static void netlink_free_socket(struct netlink_sock *nl_sk) {
    if (nl_sk == NULL) {
        return;
    }

    nl_sk->node = NULL;

    spin_lock(netlink_sockets_lock);
    for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
        if (netlink_sockets[i] == nl_sk) {
            netlink_sockets[i] = NULL;
            break;
        }
    }
    spin_unlock(netlink_sockets_lock);

    if (nl_sk->buffer != NULL) {
        free(nl_sk->buffer);
    }
    if (nl_sk->bind_addr != NULL) {
        free(nl_sk->bind_addr);
    }
    if (nl_sk->filter != NULL) {
        free(nl_sk->filter->filter);
        free(nl_sk->filter);
    }
    free(nl_sk);
}

static int netlink_poll(void *file, size_t events) {
    socket_handle_t *handle = file;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    if (!nl_sk) {
        return EPOLLERR;
    }

    int revents = 0;

    if (events & EPOLLIN) {
        if (netlink_buffer_has_msg(nl_sk)) {
            revents |= EPOLLIN;
        }
    }

    if (events & EPOLLOUT) {
        revents |= EPOLLOUT;
    }

    return revents;
}

static size_t netlink_read_op(void *file, void *buf, size_t offset,
                              size_t count) {
    UNUSED(offset);

    socket_handle_t *handle = file;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    if (!handle || !nl_sk || !nl_sk->buffer) {
        return (size_t)-EINVAL;
    }

    bool noblock = !!(handle->fd && (fd_get_flags(handle->fd) & O_NONBLOCK));
    bool has_msg = netlink_buffer_has_msg(nl_sk);
    if (!has_msg && noblock) {
        return (size_t)-EAGAIN;
    }

    while (!has_msg) {
        int reason = netlink_wait_sock(nl_sk, EPOLLIN, "netlink_read");
        if (reason != EOK) {
            return (size_t)-EINTR;
        }
        has_msg = netlink_buffer_has_msg(nl_sk);
    }

    uint32_t sender_pid = 0;
    uint32_t sender_groups = 0;
    size_t bytes = netlink_buffer_read_packet(nl_sk, (char *)buf, count,
                                              &sender_pid, &sender_groups,
                                              false);
    if (!bytes) {
        return (size_t)-EAGAIN;
    }
    return bytes;
}

static size_t netlink_write_op(void *file, const void *buf, size_t offset,
                               size_t count) {
    UNUSED(offset);

    socket_handle_t *handle = file;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    if (!handle || !nl_sk) {
        return (size_t)-EBADF;
    }

    if (nl_sk->protocol == NETLINK_KOBJECT_UEVENT) {
        return count;
    }

    return (size_t)-EDESTADDRREQ;
}

static errno_t netlink_ioctl(void *file, size_t cmd, void *arg) {
    UNUSED(arg);

    socket_handle_t *handle = file;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    if (!handle || !nl_sk) {
        return -EBADF;
    }

    switch (cmd) {
    case FIONBIO:
        return 0;
    default:
        return -ENOTTY;
    }
}

static errno_t netlinkfs_stat(void *file, vfs_node_t node) {
    socket_handle_t *handle = file;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    if (nl_sk && nl_sk->buffer) {
        node->size = netlink_buffer_available(nl_sk->buffer);
    }
    return EOK;
}

static bool netlink_close(void *current) {
    socket_handle_t *handle = current;
    if (!handle) {
        return true;
    }

    socket_t *refinfo = handle->info;
    struct netlink_sock *nl_sk = netlink_handle_sock(handle);
    vfs_node_t node = handle->node;

    if (!nl_sk) {
        if (node) {
            node->handle = NULL;
        }
        free(refinfo);
        free(handle);
        return true;
    }

    if (refinfo) {
        spin_lock(refinfo->lock);
        if (refinfo->refcount > 0) {
            refinfo->refcount--;
        }
        if (refinfo->refcount > 0) {
            spin_unlock(refinfo->lock);
            return true;
        }
        spin_unlock(refinfo->lock);
    }

    if (node) {
        node->handle = NULL;
    }

    netlink_notify_sock(nl_sk, EPOLLHUP | EPOLLRDHUP | EPOLLERR);
    netlink_free_socket(nl_sk);
    free(refinfo);
    free(handle);
    return true;
}

static void netlinkfs_open(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent);
    UNUSED(name);
    node->type = file_socket;
}

static struct vfs_callback netlinkfs_callbacks = {
    .mount = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open = netlinkfs_open,
    .close = netlink_close,
    .read = netlink_read_op,
    .write = netlink_write_op,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir = (vfs_mk_t)dummy,
    .mkfile = (vfs_mk_t)dummy,
    .link = (vfs_mk_t)dummy,
    .symlink = (vfs_mk_t)dummy,
    .stat = netlinkfs_stat,
    .ioctl = netlink_ioctl,
    .dup = (vfs_dup_t)dummy,
    .poll = netlink_poll,
    .map = (vfs_mapfile_t)dummy,
    .delete = (vfs_del_t)dummy,
    .rename = (vfs_rename_t)dummy,
    .free = (vfs_free_t)dummy,
    .chmod = (vfs_chmod_t)dummy,
    .mknod = (vfs_mknod_t)dummy,
};

int netlink_socket(int domain, int type, int protocol) {
    if (domain != AF_NETLINK) {
        return -EAFNOSUPPORT;
    }
    if (!netlink_root) {
        return -ENODEV;
    }

    struct netlink_sock *nl_sk = calloc(1, sizeof(struct netlink_sock));
    if (nl_sk == NULL) {
        return -ENOMEM;
    }

    nl_sk->domain = domain;
    nl_sk->type = type & 0xF;
    nl_sk->protocol = protocol;
    nl_sk->portid = current_task ? (uint32_t)current_task->pid : 0;
    nl_sk->groups = 0;
    nl_sk->lock = SPIN_INIT;

    nl_sk->buffer = malloc(sizeof(struct netlink_buffer));
    if (nl_sk->buffer == NULL) {
        free(nl_sk);
        return -ENOMEM;
    }
    netlink_buffer_init(nl_sk->buffer);

    char buf[20];
    sprintf(buf, "nlsock%d", netlinkfd_id++);
    vfs_node_t socknode = vfs_node_alloc(netlink_root, buf);
    if (socknode == NULL) {
        free(nl_sk->buffer);
        free(nl_sk);
        return -ENOMEM;
    }
    socknode->refcount++;
    socknode->type = file_socket;
    socknode->mode = 0700;
    socknode->fsid = netlink_socket_fsid;

    socket_handle_t *handle = calloc(1, sizeof(socket_handle_t));
    if (handle == NULL) {
        vfs_free(socknode);
        free(nl_sk->buffer);
        free(nl_sk);
        return -ENOMEM;
    }

    socket_t *refinfo = netlink_refinfo_create();
    if (refinfo == NULL) {
        free(handle);
        vfs_free(socknode);
        free(nl_sk->buffer);
        free(nl_sk);
        return -ENOMEM;
    }

    handle->op = &netlink_ops;
    handle->sock = (socket_t *)nl_sk;
    handle->info = refinfo;
    handle->node = socknode;
    socknode->handle = handle;
    nl_sk->node = socknode;
    nl_sk->refinfo = refinfo;

    spin_lock(netlink_sockets_lock);
    int slot = -1;
    for (int i = 0; i < MAX_NETLINK_SOCKETS; i++) {
        if (netlink_sockets[i] == NULL) {
            netlink_sockets[i] = nl_sk;
            slot = i;
            break;
        }
    }
    spin_unlock(netlink_sockets_lock);

    if (slot == -1) {
        socknode->handle = NULL;
        free(refinfo);
        free(handle);
        vfs_free(socknode);
        free(nl_sk->buffer);
        free(nl_sk);
        return -ENOMEM;
    }

    uint64_t flags = 0;
    if (type & O_NONBLOCK) {
        flags |= O_NONBLOCK;
    }

    int ret = -EMFILE;
    uint64_t i = 0;
    with_fd_info_lock(current_task->fd_info, {
        for (i = 0; i < (uint64_t)MAX_FD_NUM; i++) {
            if (current_task->fd_info->fds[i] == NULL) {
                break;
            }
        }

        if (i == (uint64_t)MAX_FD_NUM) {
            break;
        }

        fd_t *new_fd = netlink_fd_create(socknode, flags, !!(type & O_CLOEXEC));
        if (!new_fd) {
            ret = -ENOMEM;
            break;
        }
        new_fd->fd = (int)i;
        current_task->fd_info->fds[i] = new_fd;
        procfs_on_open_file(current_task, (int)i);
        handle->fd = new_fd;
        ret = (int)i;
    });

    if (ret < 0) {
        spin_lock(netlink_sockets_lock);
        netlink_sockets[slot] = NULL;
        spin_unlock(netlink_sockets_lock);
        socknode->handle = NULL;
        free(refinfo);
        free(handle);
        vfs_free(socknode);
        free(nl_sk->buffer);
        free(nl_sk);
        return ret;
    }

    return ret;
}

int netlink_socket_pair(int domain, int type, int protocol, int *sv) {
    UNUSED(domain);
    UNUSED(type);
    UNUSED(protocol);
    UNUSED(sv);
    return -EOPNOTSUPP;
}

void netlink_init() {
    if (netlink_root) {
        return;
    }

    netlink_socket_fsid =
        vfs_regist("netlinksockfs", &netlinkfs_callbacks, 0x4E4C534B,
                   FS_VIRTUAL_FLAGS);
    if (netlink_socket_fsid < 0) {
        return;
    }

    netlink_root = vfs_node_alloc(rootdir, ".netlinksockfs");
    if (!netlink_root) {
        return;
    }
    netlink_root->type = file_dir;
    netlink_root->fsid = netlink_socket_fsid;
    netlink_root->handle = calloc(1, sizeof(socket_handle_t));

    spin_lock(netlink_msg_pool_lock);
    for (int i = 0; i < MAX_NETLINK_MSG_POOL_SIZE; i++) {
        netlink_msg_pool[i].valid = false;
    }
    netlink_msg_pool_next = 0;
    spin_unlock(netlink_msg_pool_lock);

    regist_socket(AF_NETLINK, NULL, netlink_socket, netlink_socket_pair);
}

static int netlink_atoi(const char *s) {
    int ans = 0;
    while (isdigit(*s)) {
        ans = ans * 10 + (*s) - '0';
        ++s;
    }
    return ans;
}

void netlink_uevent_resend_by_devpath(const char *devpath) {
    if (devpath == NULL || devpath[0] == '\0') {
        return;
    }

    char buffer[NETLINK_BUFFER_SIZE];
    size_t length = NETLINK_BUFFER_SIZE;
    uint32_t nl_pid = 0;
    uint32_t nl_groups = 0;

    if (netlink_msg_pool_get_by_devpath(devpath, buffer, &length, &nl_pid,
                                        &nl_groups)) {
        uint32_t seqnum = 0;
        const char *ptr = buffer;
        while (*ptr) {
            if (strncmp(ptr, "SEQNUM=", 7) == 0) {
                seqnum = (uint32_t)netlink_atoi(ptr + 7);
                break;
            }
            ptr += strlen(ptr) + 1;
        }

        netlink_broadcast_to_group(buffer, length, nl_pid, nl_groups,
                                   NETLINK_KOBJECT_UEVENT, seqnum, devpath);
    }
}

void netlink_kernel_uevent_send(const char *buf, int len) {
    if (buf == NULL || len <= 0 || len > NETLINK_BUFFER_SIZE) {
        return;
    }

    uint32_t seqnum = 0;
    char devpath[256] = {0};

    const char *ptr = buf;
    const char *end = buf + len;
    while (ptr < end && *ptr) {
        if (strncmp(ptr, "SEQNUM=", 7) == 0) {
            seqnum = (uint32_t)netlink_atoi(ptr + 7);
        } else if (strncmp(ptr, "DEVPATH=", 8) == 0) {
            const char *val_end = strchr(ptr + 8, '\0');
            if (val_end) {
                size_t path_len = (size_t)(val_end - (ptr + 8));
                if (path_len < sizeof(devpath)) {
                    memcpy(devpath, ptr + 8, path_len);
                    devpath[path_len] = '\0';
                }
            }
        }
        ptr += strlen(ptr) + 1;
    }

    netlink_broadcast_to_group(buf, (size_t)len, 0, 1,
                               NETLINK_KOBJECT_UEVENT, seqnum, devpath);
}
