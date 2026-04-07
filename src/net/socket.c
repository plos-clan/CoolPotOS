#include "driver/ioctl.h"
#include "errno.h"
#include "fs/fds.h"
#include "fs/vfs.h"
#include "map.h"
#include "mem/frame.h"
#include "net/netlink.h"
#include "net/net_syscall.h"
#include "net/real_socket.h"
#include "net/socket.h"
#include "syscall.h"
#include "task/signal.h"
#include "task/task.h"
#include "term/klog.h"

extern socket_op_t socket_ops;

typedef pcb_t task_t;
typedef map *hashmap_t;

#define HASHMAP_INIT       NULL
#define current_task       (socket_current_process())
#define fd_info            fdts
#define spinlock_t         spin_t
#define mutex_t            spin_t
#define mutex_init(lock)   (*(lock) = SPIN_INIT)
#define mutex_lock(lock)   spin_lock(*(lock))
#define mutex_unlock(lock) spin_unlock(*(lock))
#define fd_get_flags(fd)   ((fd)->flags)
#define with_fd_info_lock(fdinfo, block)                                                           \
    do                                                                                             \
    block while (0)
#define procfs_on_open_file(task, fd)       ((void)0)
#define task_effective_tgid(task)           ((task)->pid)
#define task_commit_signal(task, sig, info) send_signal_to_process((task), (sig))
#define vfs_dup(fd)                         fd_dup((fd))
#define fd_release(fd)                      socket_fd_put((fd))
#define fd_create(node, flags, cloexec)     socket_fd_create((node), (flags), (cloexec))
#define fd_destroy(fd)                      socket_fd_destroy((fd))
#define DEFAULT_PAGE_SIZE                   PAGE_SIZE
#define MAX_FD_NUM                                                                                 \
    ((int)((current_task && current_task->fd_info) ? current_task->fd_info->fds_length : 0))
#define vfs_node_ref_get(node)         socket_node_ref_get((node))
#define vfs_node_ref_put(node, unused) socket_node_ref_put((node))

static vfs_node_t socketfs_root = NULL;

static inline pcb_t socket_current_process(void) {
    tcb_t task = get_current_task();
    return task ? task->process : NULL;
}

static inline fd_t *socket_fd_create(vfs_node_t node, uint64_t flags, bool cloexec) {
    fd_t *fd = calloc(1, sizeof(fd_t));
    if (!fd)
        return NULL;
    fd->node  = node;
    fd->flags = flags;
    if (cloexec)
        fd->flags |= O_CLOEXEC;
    fd->fd = -1;
    return fd;
}

static inline void socket_fd_destroy(fd_t *fd) {
    free(fd);
}

static inline void socket_fd_put(fd_t *fd) {
    if (!fd)
        return;
    vfs_close(fd->node);
    free(fd);
}

static inline void socket_node_ref_get(vfs_node_t node) {
    if (node)
        node->refcount++;
}

static inline void socket_node_ref_put(vfs_node_t node) {
    if (node && node->refcount > 0)
        node->refcount--;
}

static inline void *alloc_frames_bytes(size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (!pages)
        pages = 1;

    uint64_t phys = alloc_frames(pages);
    if (!phys)
        return NULL;

    void *virt = phys_to_virt(phys);
    memset(virt, 0, pages * PAGE_SIZE);
    return virt;
}

static inline void free_frames_bytes(void *ptr, size_t size) {
    if (!ptr)
        return;

    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (!pages)
        pages = 1;

    free_frames(virt_to_phys(ptr), pages);
}

static inline void *hashmap_get(hashmap_t *map0, uint64_t key) {
    return *map0 ? map_get(*map0, (void *)(uintptr_t)key) : NULL;
}

static inline int hashmap_put(hashmap_t *map0, uint64_t key, void *value) {
    if (!*map0) {
        *map0 = map_create(64);
        if (!*map0)
            return -ENOMEM;
    }

    map_set(*map0, (void *)(uintptr_t)key, value);
    return 0;
}

static inline void hashmap_remove(hashmap_t *map0, uint64_t key) {
    if (*map0)
        map_remove(*map0, (void *)(uintptr_t)key);
}

int sockfsfd_id = 0;

socket_t first_unix_socket;
static socket_t *unix_socket_list_tail = &first_unix_socket;
spinlock_t unix_socket_list_lock;
static mutex_t unix_socket_bind_lock;

int unix_socket_fsid = 0;

static hashmap_t unix_socket_bind_map = HASHMAP_INIT;

typedef struct unix_socket_bind_bucket {
    uint64_t hash;
    socket_t *head;
} unix_socket_bind_bucket_t;

static inline bool unix_socket_is_dgram_type(int type) {
    return type == SOCK_DGRAM;
}

static inline bool unix_socket_is_connected_type(int type) {
    return type == SOCK_STREAM || type == SOCK_SEQPACKET;
}

static inline bool unix_socket_type_supported(int type) {
    return unix_socket_is_connected_type(type) || unix_socket_is_dgram_type(type);
}

static inline int32_t unix_socket_cred_pid_for_task(task_t task) {
    if (!task)
        return -1;

    uint64_t pid = task_effective_tgid(task);
    if (pid > INT32_MAX)
        return -1;

    return (int32_t)pid;
}

static inline void unix_socket_fill_cred_from_task(struct ucred *cred, task_t task) {
    if (!cred) {
        return;
    }

    cred->pid = unix_socket_cred_pid_for_task(task);
    cred->uid = task ? task->uid : 0;
    cred->gid = task ? task->egid : 0;
}

static inline void unix_socket_snapshot_peer_cred(socket_t *sock, const struct ucred *cred) {
    if (!sock || !cred) {
        return;
    }

    sock->peer_cred     = *cred;
    sock->has_peer_cred = true;
}

static inline bool unix_socket_get_peer_cred(const socket_t *sock, struct ucred *cred) {
    if (!sock || !cred) {
        return false;
    }

    if (sock->has_peer_cred) {
        *cred = sock->peer_cred;
        return true;
    }

    if (!sock->peer) {
        return false;
    }

    *cred = sock->peer->cred;
    return true;
}

static int unix_socket_maybe_add_passcred(socket_t *peer, unix_socket_ancillary_t **ancillary) {
    bool peer_passcred = false;

    if (!peer || !ancillary)
        return 0;

    mutex_lock(&peer->lock);
    peer_passcred = peer->passcred;
    mutex_unlock(&peer->lock);

    if (!peer_passcred)
        return 0;

    if (!*ancillary) {
        *ancillary = calloc(1, sizeof(**ancillary));
        if (!*ancillary)
            return -ENOMEM;
    }

    if (!(*ancillary)->has_cred) {
        unix_socket_fill_cred_from_task(&(*ancillary)->cred, current_task);
        (*ancillary)->has_cred = true;
    }

    return 0;
}

static uint64_t unix_socket_name_hash(const char *name) {
    uint64_t hash = 1469598103934665603ULL;
    if (!name)
        return hash;

    while (*name) {
        hash ^= (uint8_t)*name++;
        hash *= 1099511628211ULL;
    }

    return hash;
}

static void unix_socket_unlink_bound_path(const char *path) {
    if (!path || !path[0])
        return;

    vfs_node_t node = vfs_open_nofollow(path);
    if (!node)
        return;

    vfs_delete(node);
}

static inline unix_socket_bind_bucket_t *unix_socket_bind_bucket_lookup_locked(uint64_t hash) {
    return (unix_socket_bind_bucket_t *)hashmap_get(&unix_socket_bind_map, hash);
}

static socket_t *
unix_socket_lookup_bound_locked(const char *name, size_t len, socket_t *skip, bool take_node_ref) {
    if (!name || !len)
        return NULL;

    uint64_t hash                     = unix_socket_name_hash(name);
    unix_socket_bind_bucket_t *bucket = unix_socket_bind_bucket_lookup_locked(hash);
    socket_t *sock                    = bucket ? bucket->head : NULL;
    while (sock) {
        if (sock != skip && sock->bindHash == hash && sock->bindAddr && sock->bindAddrLen == len
            && memcmp(sock->bindAddr, name, len) == 0) {
            if (take_node_ref && sock->node)
                vfs_node_ref_get(sock->node);
            return sock;
        }
        sock = sock->bind_next;
    }

    return NULL;
}

static socket_t *
unix_socket_lookup_bound(const char *name, size_t len, socket_t *skip, bool take_node_ref) {
    socket_t *sock = NULL;

    mutex_lock(&unix_socket_bind_lock);
    sock = unix_socket_lookup_bound_locked(name, len, skip, take_node_ref);
    mutex_unlock(&unix_socket_bind_lock);

    return sock;
}

static inline void unix_socket_release_lookup_ref(socket_t *sock) {
    if (sock && sock->node)
        vfs_node_ref_put(sock->node, NULL);
}

char *unix_socket_addr_safe(const struct sockaddr_un *addr, size_t len) {
    ssize_t addrLen = len - sizeof(addr->sun_family);
    if (addrLen <= 0)
        return (void *)-EINVAL;

    bool abstract = (addr->sun_path[0] == '\0');
    int skip      = abstract ? 1 : 0;

    char *safe = malloc(addrLen + 3);
    if (!safe)
        return (void *)-(ENOMEM);
    memset(safe, 0, addrLen + 3);

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

static inline socket_t *socket_from_node(vfs_node_t node) {
    socket_handle_t *handle = node ? node->handle : NULL;
    return handle ? handle->sock : NULL;
}

static inline void socket_pending_mark(socket_t *sock, uint32_t events) {
    if (!sock || !events)
        return;
    __atomic_fetch_or(&sock->pending_events, events, __ATOMIC_RELEASE);
}

static inline uint32_t socket_pending_take(socket_t *sock, uint32_t events) {
    if (!sock || !events)
        return 0;

    uint32_t old_mask = 0;
    uint32_t new_mask = 0;
    do {
        old_mask = __atomic_load_n(&sock->pending_events, __ATOMIC_ACQUIRE);
        if (!(old_mask & events))
            return 0;
        new_mask = old_mask & ~events;
    } while (!__atomic_compare_exchange_n(
        &sock->pending_events, &old_mask, new_mask, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE
    ));

    return old_mask & events;
}

static inline void socket_notify_node(vfs_node_t node, uint32_t events) {
    if (!node || !events)
        return;
    vfs_poll_notify(node, events);
}

static inline void socket_notify_sock(socket_t *sock, uint32_t events) {
    if (!sock)
        return;
    socket_pending_mark(sock, events);
    socket_notify_node(sock->node, events);
}

static inline size_t unix_socket_recv_used_locked(const socket_t *sock) {
    return sock ? sock->recv_pos : 0;
}

static inline size_t unix_socket_recv_space_locked(const socket_t *sock) {
    if (!sock || sock->recv_pos >= sock->recv_size)
        return 0;
    return sock->recv_size - sock->recv_pos;
}

static size_t unix_socket_recv_write_locked(socket_t *sock, const uint8_t *data, size_t len) {
    if (!sock || !data || !len)
        return 0;

    size_t to_copy = MIN(len, unix_socket_recv_space_locked(sock));
    if (!to_copy)
        return 0;

    size_t tail  = (sock->recv_head + sock->recv_pos) % sock->recv_size;
    size_t first = MIN(to_copy, sock->recv_size - tail);
    memcpy(sock->recv_buff + tail, data, first);
    if (to_copy > first) {
        memcpy(sock->recv_buff, data + first, to_copy - first);
    }

    sock->recv_pos += to_copy;
    return to_copy;
}

static size_t
unix_socket_recv_copy_out_locked(const socket_t *sock, size_t start, uint8_t *out, size_t len) {
    if (!sock || !out || !len || !sock->recv_size)
        return 0;

    size_t head  = (sock->recv_head + start) % sock->recv_size;
    size_t first = MIN(len, sock->recv_size - head);
    memcpy(out, sock->recv_buff + head, first);
    if (len > first) {
        memcpy(out + first, sock->recv_buff, len - first);
    }

    return len;
}

static size_t unix_socket_recv_read_locked(socket_t *sock, uint8_t *out, size_t len, bool peek) {
    if (!sock || !out || !len)
        return 0;

    size_t to_copy = MIN(len, unix_socket_recv_used_locked(sock));
    if (!to_copy)
        return 0;

    unix_socket_recv_copy_out_locked(sock, 0, out, to_copy);

    if (!peek) {
        sock->recv_head = (sock->recv_head + to_copy) % sock->recv_size;
        sock->recv_pos -= to_copy;
        if (!sock->recv_pos)
            sock->recv_head = 0;
    }

    return to_copy;
}

static size_t unix_socket_recv_readv_locked(
    socket_t *sock, const struct iovec *iov, size_t iovlen, size_t len_total, bool peek
) {
    if (!sock || !iov || !iovlen || !len_total)
        return 0;

    size_t remaining = MIN(len_total, unix_socket_recv_used_locked(sock));
    size_t consumed  = 0;

    for (size_t i = 0; i < iovlen && remaining > 0; i++) {
        if (!iov[i].iov_base || !iov[i].len)
            continue;

        size_t copy_len = MIN(iov[i].len, remaining);
        unix_socket_recv_copy_out_locked(sock, consumed, iov[i].iov_base, copy_len);
        consumed += copy_len;
        remaining -= copy_len;
    }

    if (!peek && consumed > 0) {
        sock->recv_head = (sock->recv_head + consumed) % sock->recv_size;
        sock->recv_pos -= consumed;
        if (!sock->recv_pos)
            sock->recv_head = 0;
    }

    return consumed;
}

static bool unix_socket_backlog_reserve_locked(socket_t *sock, int min_capacity) {
    if (!sock || min_capacity <= 0)
        return true;

    if (sock->backlogCap >= min_capacity)
        return true;

    if (sock->connMax <= 0 || min_capacity > sock->connMax)
        return false;

    int new_capacity = sock->backlogCap;
    if (new_capacity <= 0)
        new_capacity = MIN(sock->connMax, 16);

    while (new_capacity < min_capacity) {
        if (new_capacity >= sock->connMax) {
            new_capacity = sock->connMax;
            break;
        }

        if (new_capacity > sock->connMax / 2) {
            new_capacity = sock->connMax;
        } else {
            new_capacity *= 2;
        }
    }

    if (new_capacity < min_capacity)
        return false;

    socket_t **new_backlog = calloc((size_t)new_capacity, sizeof(*new_backlog));
    if (!new_backlog)
        return false;

    for (int i = 0; i < sock->connCurr; i++) {
        int slot       = (sock->connHead + i) % sock->backlogCap;
        new_backlog[i] = sock->backlog[slot];
    }

    free(sock->backlog);
    sock->backlog    = new_backlog;
    sock->backlogCap = new_capacity;
    sock->connHead   = 0;
    return true;
}

static bool unix_socket_backlog_enqueue_tail_locked(socket_t *sock, socket_t *pending_sock) {
    if (!sock || !pending_sock)
        return false;
    if (!unix_socket_backlog_reserve_locked(sock, sock->connCurr + 1))
        return false;

    int tail            = (sock->connHead + sock->connCurr) % sock->backlogCap;
    sock->backlog[tail] = pending_sock;
    sock->connCurr++;
    return true;
}

static bool unix_socket_backlog_enqueue_head_locked(socket_t *sock, socket_t *pending_sock) {
    if (!sock || !pending_sock)
        return false;
    if (!unix_socket_backlog_reserve_locked(sock, sock->connCurr + 1))
        return false;

    if (sock->connCurr == 0) {
        sock->connHead = 0;
    } else {
        sock->connHead = (sock->connHead + sock->backlogCap - 1) % sock->backlogCap;
    }
    sock->backlog[sock->connHead] = pending_sock;
    sock->connCurr++;
    return true;
}

static bool unix_socket_requeue_pending_accept(socket_t *listen_sock, socket_t *server_sock) {
    bool requeued = false;

    if (!listen_sock || !server_sock)
        return false;

    mutex_lock(&listen_sock->lock);
    if (!listen_sock->closed && listen_sock->connMax > 0)
        requeued = unix_socket_backlog_enqueue_head_locked(listen_sock, server_sock);
    mutex_unlock(&listen_sock->lock);

    if (requeued)
        socket_notify_sock(listen_sock, EPOLLIN);
    return requeued;
}

static void unix_socket_ancillary_free(unix_socket_ancillary_t *ancillary) {
    if (!ancillary)
        return;

    for (uint32_t i = 0; i < ancillary->file_count; i++) {
        if (ancillary->files[i])
            fd_release(ancillary->files[i]);
    }

    free(ancillary);
}

static void unix_socket_ancillary_free_list(unix_socket_ancillary_t *ancillary_list) {
    while (ancillary_list) {
        unix_socket_ancillary_t *next = ancillary_list->next;
        unix_socket_ancillary_free(ancillary_list);
        ancillary_list = next;
    }
}

static void unix_socket_ancillary_enqueue_locked(socket_t *sock, unix_socket_ancillary_t *anc) {
    if (!sock || !anc)
        return;

    anc->next = NULL;
    if (sock->ancillary_tail) {
        sock->ancillary_tail->next = anc;
    } else {
        sock->ancillary_head = anc;
    }
    sock->ancillary_tail = anc;
}

static void unix_socket_ancillary_drop_before_locked(socket_t *sock, uint64_t seq_limit) {
    if (!sock)
        return;

    while (sock->ancillary_head && sock->ancillary_head->seq < seq_limit) {
        unix_socket_ancillary_t *stale = sock->ancillary_head;
        sock->ancillary_head           = stale->next;
        if (!sock->ancillary_head)
            sock->ancillary_tail = NULL;
        stale->next = NULL;
        unix_socket_ancillary_free(stale);
    }
}

static unix_socket_ancillary_t *
unix_socket_ancillary_clone_one(const unix_socket_ancillary_t *src) {
    if (!src)
        return NULL;

    unix_socket_ancillary_t *clone = calloc(1, sizeof(*clone));
    if (!clone)
        return NULL;

    clone->seq        = src->seq;
    clone->file_count = src->file_count;
    clone->cred       = src->cred;
    clone->has_cred   = src->has_cred;

    for (uint32_t i = 0; i < src->file_count; i++) {
        clone->files[i] = vfs_dup(src->files[i]);
        if (!clone->files[i]) {
            unix_socket_ancillary_free(clone);
            return NULL;
        }
    }

    return clone;
}

static size_t unix_socket_iov_total_len(const struct iovec *iov, size_t iovlen) {
    size_t total = 0;

    if (!iov)
        return 0;

    for (size_t i = 0; i < iovlen; i++)
        total += iov[i].len;

    return total;
}

static size_t unix_socket_stream_read_limit_locked(const socket_t *sock, size_t requested) {
    size_t limit = MIN(requested, unix_socket_recv_used_locked(sock));
    if (!sock || !limit)
        return limit;

    unix_socket_ancillary_t *ancillary = sock->ancillary_head;
    while (ancillary && ancillary->seq < sock->recv_seq)
        ancillary = ancillary->next;

    if (ancillary && ancillary->seq < sock->recv_seq + limit)
        limit = (size_t)(ancillary->seq - sock->recv_seq + 1);

    return limit;
}

static int
unix_socket_prepare_ancillary(const struct msghdr *msg, unix_socket_ancillary_t **out_anc) {
    if (!out_anc)
        return -EINVAL;

    *out_anc = NULL;
    if (!msg || !msg->msg_control || msg->msg_controllen == 0)
        return 0;

    unix_socket_ancillary_t *anc = calloc(1, sizeof(*anc));
    if (!anc)
        return -ENOMEM;

    bool have_rights = false;
    bool have_cred   = false;

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
         cmsg                 = CMSG_NXTHDR((struct msghdr *)msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET)
            continue;

        if (cmsg->cmsg_type == SCM_RIGHTS) {
            if (have_rights || cmsg->cmsg_len < CMSG_LEN(sizeof(int))) {
                unix_socket_ancillary_free(anc);
                return -EINVAL;
            }

            size_t rights_len = cmsg->cmsg_len - CMSG_LEN(0);
            if ((rights_len % sizeof(int)) != 0) {
                unix_socket_ancillary_free(anc);
                return -EINVAL;
            }

            uint32_t file_count = rights_len / sizeof(int);
            if (file_count == 0 || file_count > MAX_PENDING_FILES_COUNT) {
                unix_socket_ancillary_free(anc);
                return -ETOOMANYREFS;
            }

            int *fds = (int *)CMSG_DATA(cmsg);
            for (uint32_t i = 0; i < file_count; i++) {
                int send_fd = fds[i];
                if (send_fd < 0 || send_fd >= MAX_FD_NUM || !current_task->fd_info->fds[send_fd]) {
                    unix_socket_ancillary_free(anc);
                    return -EBADF;
                }

                anc->files[anc->file_count] = vfs_dup(current_task->fd_info->fds[send_fd]);
                if (!anc->files[anc->file_count]) {
                    unix_socket_ancillary_free(anc);
                    return -ENOMEM;
                }
                anc->file_count++;
            }

            have_rights = true;
        } else if (cmsg->cmsg_type == SCM_CREDENTIALS) {
            if (have_cred || cmsg->cmsg_len < CMSG_LEN(sizeof(struct ucred))) {
                unix_socket_ancillary_free(anc);
                return -EINVAL;
            }

            struct ucred *cred = (struct ucred *)CMSG_DATA(cmsg);
            if (current_task->euid != 0
                && (cred->pid != unix_socket_cred_pid_for_task(current_task)
                    || cred->uid != current_task->uid || cred->gid != current_task->egid)) {
                unix_socket_ancillary_free(anc);
                return -EPERM;
            }

            anc->cred     = *cred;
            anc->has_cred = true;
            have_cred     = true;
        }
    }

    if (!anc->file_count && !anc->has_cred) {
        free(anc);
        return 0;
    }

    *out_anc = anc;
    return 0;
}

static int unix_socket_collect_ancillary_locked(
    socket_t *sock, uint64_t end_seq, bool peek, unix_socket_ancillary_t **out
) {
    if (!out)
        return -EINVAL;

    *out = NULL;
    if (!sock)
        return 0;

    unix_socket_ancillary_t *list = NULL;
    unix_socket_ancillary_t *tail = NULL;

    if (peek) {
        for (unix_socket_ancillary_t *curr = sock->ancillary_head; curr && curr->seq < end_seq;
             curr                          = curr->next) {
            unix_socket_ancillary_t *clone = unix_socket_ancillary_clone_one(curr);
            if (!clone) {
                unix_socket_ancillary_free_list(list);
                return -ENOMEM;
            }

            if (tail) {
                tail->next = clone;
            } else {
                list = clone;
            }
            tail = clone;
        }
    } else {
        while (sock->ancillary_head && sock->ancillary_head->seq < end_seq) {
            unix_socket_ancillary_t *curr = sock->ancillary_head;
            sock->ancillary_head          = curr->next;
            if (!sock->ancillary_head)
                sock->ancillary_tail = NULL;
            curr->next = NULL;

            if (tail) {
                tail->next = curr;
            } else {
                list = curr;
            }
            tail = curr;
        }
    }

    *out = list;
    return 0;
}

static int socket_wait_node(vfs_node_t node, uint32_t events, const char *reason) {
    if (!node || !current_task)
        return -EINVAL;

    socket_t *wait_sock = socket_from_node(node);
    uint32_t want       = events | EPOLLERR | EPOLLHUP | EPOLLNVAL | EPOLLRDHUP;
    if (socket_pending_take(wait_sock, want))
        return EOK;
    int polled = vfs_poll(node, want);
    if (polled < 0)
        return polled;
    if (polled & (int)want)
        return EOK;

    vfs_poll_wait_t wait;
    vfs_poll_wait_init(&wait, get_current_task(), want);
    if (vfs_poll_wait_arm(node, &wait) < 0)
        return -EINVAL;

    if (socket_pending_take(wait_sock, want)) {
        vfs_poll_wait_disarm(&wait);
        return EOK;
    }

    polled = vfs_poll(node, want);
    if (polled < 0) {
        vfs_poll_wait_disarm(&wait);
        return polled;
    }
    if (polled & (int)want) {
        vfs_poll_wait_disarm(&wait);
        return EOK;
    }

    int ret = vfs_poll_wait_sleep(node, &wait, -1, reason);
    vfs_poll_wait_disarm(&wait);
    return ret;
}

static const char *unix_socket_local_name(const socket_t *sock) {
    if (!sock)
        return "";
    if (sock->bindAddr && sock->bindAddr[0])
        return sock->bindAddr;
    if (sock->filename && sock->filename[0])
        return sock->filename;
    return "";
}

static void
unix_socket_write_sockaddr(const char *name, struct sockaddr_un *addr, socklen_t *addrlen) {
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = 1;
    *addrlen         = sizeof(addr->sun_family);

    if (!name || !name[0])
        return;

    size_t max_path = sizeof(addr->sun_path);
    size_t raw_len  = strlen(name);

    if (name[0] == '@') {
        size_t n          = MIN(raw_len - 1, max_path - 1);
        addr->sun_path[0] = '\0';
        if (n > 0)
            memcpy(addr->sun_path + 1, name + 1, n);
        *addrlen += 1 + n;
    } else {
        size_t n = MIN(raw_len, max_path - 1);
        memcpy(addr->sun_path, name, n);
        *addrlen += n + 1;
    }
}

socket_t *unix_socket_alloc() {
    socket_t *sock = malloc(sizeof(socket_t));
    if (!sock)
        return NULL;
    memset(sock, 0, sizeof(socket_t));
    mutex_init(&sock->lock);

    sock->recv_size = BUFFER_SIZE;
    sock->recv_buff = alloc_frames_bytes(BUFFER_SIZE);
    if (!sock->recv_buff) {
        free(sock);
        return NULL;
    }
    sock->recv_head      = 0;
    sock->recv_pos       = 0;
    sock->recv_seq       = 0;
    sock->node           = NULL;
    sock->refcount       = 1;
    sock->ancillary_head = NULL;
    sock->ancillary_tail = NULL;

    memset(sock->pending_files, 0, sizeof(sock->pending_files));
    sock->has_pending_cred = false;

    // 设置凭据
    unix_socket_fill_cred_from_task(&sock->cred, current_task);

    // 加入链表
    spin_lock(unix_socket_list_lock);
    unix_socket_list_tail->next = sock;
    unix_socket_list_tail       = sock;
    spin_unlock(unix_socket_list_lock);

    return sock;
}

void unix_socket_free(socket_t *sock) {
    if (!sock)
        return;

    if (sock->bindAddr) {
        mutex_lock(&unix_socket_bind_lock);
        unix_socket_bind_bucket_t *bucket = unix_socket_bind_bucket_lookup_locked(sock->bindHash);
        socket_t *bind_head               = bucket ? bucket->head : NULL;
        socket_t *prev                    = NULL;
        socket_t *curr                    = bind_head;
        while (curr && curr != sock) {
            prev = curr;
            curr = curr->bind_next;
        }
        if (curr == sock) {
            if (prev) {
                prev->bind_next = curr->bind_next;
            } else {
                if (bucket)
                    bucket->head = curr->bind_next;
                if (!bucket || !bucket->head) {
                    hashmap_remove(&unix_socket_bind_map, sock->bindHash);
                    free(bucket);
                }
            }
            curr->bind_next = NULL;
        }
        mutex_unlock(&unix_socket_bind_lock);
    }

    // 从链表移除
    spin_lock(unix_socket_list_lock);
    socket_t *browse = &first_unix_socket;
    while (browse && browse->next != sock)
        browse = browse->next;
    if (browse) {
        browse->next = sock->next;
        if (unix_socket_list_tail == sock)
            unix_socket_list_tail = browse;
    }
    spin_unlock(unix_socket_list_lock);

    // 释放资源
    if (sock->recv_buff)
        free_frames_bytes(sock->recv_buff, sock->recv_size);
    if (sock->bindAddr)
        free(sock->bindAddr);
    if (sock->filename)
        free(sock->filename);
    if (sock->backlog)
        free(sock->backlog);
    if (sock->filter)
        free(sock->filter);
    unix_socket_ancillary_free_list(sock->ancillary_head);

    // 清理 pending files
    for (int i = 0; i < MAX_PENDING_FILES_COUNT; i++) {
        if (sock->pending_files[i]) {
            fd_release(sock->pending_files[i]);
        }
    }

    free(sock);
}

// 发送数据到对端的 recv_buff
static size_t unix_socket_send_to_peer(
    socket_t *self,
    socket_t *peer,
    const uint8_t *data,
    size_t len,
    int flags,
    fd_t *fd_handle,
    unix_socket_ancillary_t **ancillary
) {
    socket_t *active_peer = peer;
    if (self && !unix_socket_is_dgram_type(self->type))
        active_peer = self->peer;

    if (self && self->shut_wr) {
        if (!(flags & MSG_NOSIGNAL))
            task_commit_signal(current_task, SIGPIPE, NULL);
        return -EPIPE;
    }

    if (!active_peer || active_peer->closed || active_peer->shut_rd) {
        if (!(flags & MSG_NOSIGNAL))
            task_commit_signal(current_task, SIGPIPE, NULL);
        return -EPIPE;
    }

    if (!len)
        return 0;

    while (true) {
        if (self && !unix_socket_is_dgram_type(self->type))
            active_peer = self->peer;
        if (!active_peer) {
            if (!(flags & MSG_NOSIGNAL))
                task_commit_signal(current_task, SIGPIPE, NULL);
            return -EPIPE;
        }

        mutex_lock(&active_peer->lock);
        if (active_peer->closed || active_peer->shut_rd) {
            mutex_unlock(&active_peer->lock);
            if (!(flags & MSG_NOSIGNAL))
                task_commit_signal(current_task, SIGPIPE, NULL);
            return -EPIPE;
        }
        size_t available = unix_socket_recv_space_locked(active_peer);
        if (available > 0) {
            if (ancillary && *ancillary) {
                (*ancillary)->seq = active_peer->recv_seq + active_peer->recv_pos;
                unix_socket_ancillary_enqueue_locked(active_peer, *ancillary);
                *ancillary = NULL;
            }
            size_t to_copy = unix_socket_recv_write_locked(active_peer, data, len);
            mutex_unlock(&active_peer->lock);
            socket_notify_sock(active_peer, EPOLLIN);
            return to_copy;
        }
        mutex_unlock(&active_peer->lock);

        if ((fd_handle && (fd_get_flags(fd_handle) & O_NONBLOCK)) || (flags & MSG_DONTWAIT)) {
            return -(EWOULDBLOCK);
        }

        vfs_node_t wait_node = NULL;
        if (self && !unix_socket_is_dgram_type(self->type) && self->node)
            wait_node = self->node;
        if (!wait_node && active_peer->node)
            wait_node = active_peer->node;
        if (!wait_node)
            return -EINVAL;
        int reason = socket_wait_node(wait_node, EPOLLOUT, "socket_send");
        if (reason != EOK)
            return -EINTR;
    }

    return 0;
}

// 从自己的 recv_buff 接收数据
static size_t unix_socket_recv_from_self(
    socket_t *self, socket_t *peer, uint8_t *buf, size_t len, int flags, fd_t *fd_handle
) {
    bool peek = !!(flags & MSG_PEEK);

    if (self->shut_rd)
        return 0;
    if (!len)
        return 0;

    // 等待数据
    while (true) {
        mutex_lock(&self->lock);

        if (self->recv_pos > 0) {
            size_t limit = len;
            if (!unix_socket_is_dgram_type(self->type))
                limit = unix_socket_stream_read_limit_locked(self, len);
            size_t to_copy = unix_socket_recv_read_locked(self, buf, limit, peek);
            if (!peek) {
                self->recv_seq += to_copy;
                unix_socket_ancillary_drop_before_locked(self, self->recv_seq);
            }
            mutex_unlock(&self->lock);
            if (!peek) {
                socket_notify_sock(self, EPOLLOUT);
                if (self->peer)
                    socket_notify_sock(self->peer, EPOLLOUT);
            }
            return to_copy;
        }

        socket_t *active_peer = peer;
        if (!unix_socket_is_dgram_type(self->type))
            active_peer = self->peer;
        bool eof = (!active_peer || active_peer->closed || active_peer->shut_wr);
        mutex_unlock(&self->lock);

        // 对端关闭且没有数据 = EOF
        if (eof) {
            return 0;
        }

        if ((fd_handle && (fd_get_flags(fd_handle) & O_NONBLOCK)) || (flags & MSG_DONTWAIT)) {
            return -(EWOULDBLOCK);
        }

        if (!self->node)
            return -EINVAL;
        int reason = socket_wait_node(self->node, EPOLLIN, "socket_recv");
        if (reason != EOK)
            return -EINTR;
    }
}

static size_t unix_socket_recvmsg_from_self(
    socket_t *self,
    socket_t *peer,
    struct msghdr *msg,
    int flags,
    fd_t *fd_handle,
    uint64_t *start_seq_out
) {
    bool peek = !!(flags & MSG_PEEK);

    if (!self || !msg)
        return -EINVAL;
    if (self->shut_rd)
        return 0;

    size_t len_total = unix_socket_iov_total_len(msg->msg_iov, msg->msg_iovlen);

    if (!len_total)
        return 0;

    while (true) {
        mutex_lock(&self->lock);

        if (self->recv_pos > 0) {
            size_t limit = len_total;
            if (!unix_socket_is_dgram_type(self->type))
                limit = unix_socket_stream_read_limit_locked(self, len_total);
            uint64_t start_seq = self->recv_seq;
            size_t copied =
                unix_socket_recv_readv_locked(self, msg->msg_iov, msg->msg_iovlen, limit, peek);
            if (!peek) {
                self->recv_seq += copied;
                unix_socket_ancillary_drop_before_locked(self, self->recv_seq);
            }
            if (start_seq_out)
                *start_seq_out = start_seq;
            mutex_unlock(&self->lock);
            if (!peek) {
                socket_notify_sock(self, EPOLLOUT);
                if (self->peer)
                    socket_notify_sock(self->peer, EPOLLOUT);
            }
            return copied;
        }

        socket_t *active_peer = peer;
        if (!unix_socket_is_dgram_type(self->type))
            active_peer = self->peer;
        bool eof = (!active_peer || active_peer->closed || active_peer->shut_wr);
        mutex_unlock(&self->lock);

        if (eof)
            return 0;

        if ((fd_handle && (fd_get_flags(fd_handle) & O_NONBLOCK)) || (flags & MSG_DONTWAIT)) {
            return -(EWOULDBLOCK);
        }

        if (!self->node)
            return -EINVAL;
        int reason = socket_wait_node(self->node, EPOLLIN, "socket_recvmsg");
        if (reason != EOK)
            return -EINTR;
    }
}

// 发送 pending files 到对端
static int unix_socket_send_files_to_peer(socket_t *peer, int *fds, int num_fds) {
    if (!peer)
        return -EINVAL;

    int free_slots = 0;
    for (int i = 0; i < MAX_PENDING_FILES_COUNT; i++) {
        if (!peer->pending_files[i])
            free_slots++;
    }
    if (free_slots < num_fds)
        return -ETOOMANYREFS;

    for (int i = 0; i < num_fds; i++) {
        int fd = fds[i];
        if (fd < 0 || fd >= MAX_FD_NUM)
            return -EBADF;
        if (!current_task->fd_info->fds[fd])
            return -EBADF;

        bool inserted = false;
        for (int j = 0; j < MAX_PENDING_FILES_COUNT; j++) {
            if (peer->pending_files[j] == NULL) {
                peer->pending_files[j] = vfs_dup(current_task->fd_info->fds[fd]);
                if (!peer->pending_files[j])
                    return -ENOMEM;
                inserted = true;
                break;
            }
        }
        if (!inserted)
            return -ETOOMANYREFS;
    }
    socket_notify_sock(peer, EPOLLIN);
    return 0;
}

static void unix_socket_drop_pending_file(fd_t *pending_file) {
    if (!pending_file)
        return;
    fd_release(pending_file);
}

// 该函数要求调用者持有 self->lock
static size_t
unix_socket_take_pending_files_locked(socket_t *self, fd_t **pending_files, size_t max_fds) {
    size_t taken = 0;

    for (int i = 0; i < MAX_PENDING_FILES_COUNT && taken < max_fds; i++) {
        if (!self->pending_files[i])
            continue;
        pending_files[taken++] = self->pending_files[i];
        self->pending_files[i] = NULL;
    }

    return taken;
}

static size_t unix_socket_install_pending_files(
    fd_t **pending_files, size_t pending_count, int *fds_out, int *msg_flags, int recv_flags
) {
    size_t installed = 0;
    with_fd_info_lock(current_task->fd_info, {
        for (size_t i = 0; i < pending_count; i++) {
            int new_fd = -1;
            for (int fd_idx = 0; fd_idx < MAX_FD_NUM; fd_idx++) {
                if (current_task->fd_info->fds[fd_idx] == NULL) {
                    new_fd = fd_idx;
                    break;
                }
            }

            if (new_fd < 0)
                break;

            fd_t *new_entry = vfs_dup(pending_files[i]);
            if (!new_entry)
                break;
            if (recv_flags & MSG_CMSG_CLOEXEC)
                new_entry->flags |= O_CLOEXEC;
            current_task->fd_info->fds[new_fd] = new_entry;
            fds_out[installed++]               = new_fd;
        }
    });

    for (size_t i = 0; i < installed; i++) {
        fd_release(pending_files[i]);
        pending_files[i] = NULL;
        procfs_on_open_file(current_task, fds_out[i]);
    }

    if (installed < pending_count) {
        if (msg_flags)
            *msg_flags |= MSG_CTRUNC;
        for (size_t i = installed; i < pending_count; i++) {
            unix_socket_drop_pending_file(pending_files[i]);
            pending_files[i] = NULL;
        }
    }

    return installed;
}

// 发送凭据到对端
static void unix_socket_send_cred_to_peer(socket_t *peer, struct ucred *cred) {
    if (!peer)
        return;
    memcpy(&peer->pending_cred, cred, sizeof(struct ucred));
    peer->has_pending_cred = true;
    socket_notify_sock(peer, EPOLLIN);
}

vfs_node_t unix_socket_create_node(socket_t *sock) {
    char buf[16];
    sprintf(buf, "sock%d", sockfsfd_id++);
    vfs_node_t socknode = vfs_node_alloc(socketfs_root, buf);
    if (!socknode)
        return NULL;
    socknode->refcount++;
    socknode->type = file_socket;
    socknode->mode = 0700;
    socknode->fsid = unix_socket_fsid;

    socket_handle_t *handle = malloc(sizeof(socket_handle_t));
    if (!handle) {
        vfs_free(socknode);
        return NULL;
    }
    memset(handle, 0, sizeof(socket_handle_t));
    handle->op   = &socket_ops;
    handle->sock = sock;

    socknode->handle = handle;
    handle->info     = sock;
    handle->node     = socknode;
    sock->node       = socknode;
    return socknode;
}

int socket_socket(int domain, int type, int protocol) {
    int sock_type = type & 0xF;
    if (!unix_socket_type_supported(sock_type)) {
        return -ESOCKTNOSUPPORT;
    }

    socket_t *sock = unix_socket_alloc();
    if (!sock)
        return -ENOMEM;

    sock->domain   = domain;
    sock->type     = sock_type;
    sock->protocol = protocol;

    vfs_node_t socknode = unix_socket_create_node(sock);
    if (!socknode) {
        unix_socket_free(sock);
        return -ENOMEM;
    }
    socket_handle_t *handle = socknode->handle;

    int ret        = -EMFILE;
    uint64_t i     = 0;
    uint64_t flags = O_RDWR;
    with_fd_info_lock(current_task->fd_info, {
        for (i = 0; i < MAX_FD_NUM; i++) {
            if (current_task->fd_info->fds[i] == NULL)
                break;
        }

        if (i == MAX_FD_NUM)
            break;

        if (type & O_NONBLOCK)
            flags |= O_NONBLOCK;
        fd_t *new_fd = fd_create(socknode, flags, !!(type & O_CLOEXEC));
        if (!new_fd) {
            ret = -ENOMEM;
            break;
        }

        new_fd->fd                    = (int)i;
        current_task->fd_info->fds[i] = new_fd;
        procfs_on_open_file(current_task, i);
        ret = (int)i;
    });

    if (ret < 0) {
        unix_socket_free(sock);
        vfs_free(socknode);
        return ret;
    }

    handle->fd = current_task->fd_info->fds[i];

    return ret;
}

int socket_bind(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen) {
    if (!addr)
        return -EFAULT;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    if (sock->bindAddr)
        return -EINVAL;

    char *safe = unix_socket_addr_safe(addr, addrlen);
    if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK)
        return (uint64_t)safe;

    bool is_abstract = (addr->sun_path[0] == '\0');
    size_t safeLen   = strlen(safe);

    if (!is_abstract) {
        vfs_node_t existing = vfs_open(safe);
        if (existing) {
            vfs_close(existing);
            free(safe);
            return -EADDRINUSE;
        }
        int mkret = vfs_mknod(safe, S_IFSOCK | 0666, 0);
        if (mkret < 0) {
            free(safe);
            return mkret;
        }
    }

    uint64_t bind_hash = unix_socket_name_hash(safe);
    mutex_lock(&unix_socket_bind_lock);
    if (unix_socket_lookup_bound_locked(safe, safeLen, sock, false)) {
        mutex_unlock(&unix_socket_bind_lock);
        free(safe);
        return -EADDRINUSE;
    }

    unix_socket_bind_bucket_t *bucket = unix_socket_bind_bucket_lookup_locked(bind_hash);
    if (!bucket) {
        bucket = calloc(1, sizeof(*bucket));
        if (!bucket) {
            mutex_unlock(&unix_socket_bind_lock);
            if (!is_abstract)
                unix_socket_unlink_bound_path(safe);
            free(safe);
            return -ENOMEM;
        }
        bucket->hash = bind_hash;
        if (hashmap_put(&unix_socket_bind_map, bind_hash, bucket) != 0) {
            free(bucket);
            mutex_unlock(&unix_socket_bind_lock);
            if (!is_abstract)
                unix_socket_unlink_bound_path(safe);
            free(safe);
            return -ENOMEM;
        }
    }

    sock->bindAddr    = safe;
    sock->bindAddrLen = safeLen;
    sock->bindHash    = bind_hash;
    sock->bind_next   = bucket->head;
    bucket->head      = sock;
    mutex_unlock(&unix_socket_bind_lock);

    return 0;
}

int socket_listen(uint64_t fd, int backlog) {
    if (backlog == 0)
        backlog = 16;
    if (backlog < 0)
        backlog = 0;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    mutex_lock(&sock->lock);
    unix_socket_fill_cred_from_task(&sock->cred, current_task);
    if (sock->backlog) {
        free(sock->backlog);
        sock->backlog = NULL;
    }
    sock->connMax    = backlog;
    sock->connCurr   = 0;
    sock->connHead   = 0;
    sock->backlogCap = 0;
    mutex_unlock(&sock->lock);
    return 0;
}

int socket_accept(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen, uint64_t flags) {
    if (fd >= MAX_FD_NUM) {
        return -EBADF;
    }

    fd_t *listener_fd = NULL;
    with_fd_info_lock(current_task->fd_info, {
        if (current_task->fd_info->fds[fd]) {
            listener_fd = vfs_dup(current_task->fd_info->fds[fd]);
        }
    });
    if (!listener_fd) {
        return -EBADF;
    }

    socket_handle_t *handle = listener_fd->node->handle;
    socket_t *listen_sock   = handle->sock;

    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        fd_release(listener_fd);
        return -EINVAL;
    }

    if (addr && !addrlen) {
        fd_release(listener_fd);
        return -EFAULT;
    }

    bool listener_nonblock = !!(fd_get_flags(listener_fd) & O_NONBLOCK);

    if (!listen_sock->connMax) {
        fd_release(listener_fd);
        return -EINVAL;
    }

    // 等待连接并从 backlog 取一个
    socket_t *server_sock = NULL;
    while (true) {
        mutex_lock(&listen_sock->lock);
        if (listen_sock->connCurr > 0) {
            int head                   = listen_sock->connHead;
            server_sock                = listen_sock->backlog[head];
            listen_sock->backlog[head] = NULL;
            listen_sock->connHead      = (listen_sock->connHead + 1) % listen_sock->backlogCap;
            listen_sock->connCurr--;
            if (listen_sock->connCurr == 0)
                listen_sock->connHead = 0;
            mutex_unlock(&listen_sock->lock);
            socket_notify_sock(listen_sock, EPOLLOUT);
            break;
        }
        mutex_unlock(&listen_sock->lock);
        if (fd_get_flags(listener_fd) & O_NONBLOCK) {
            fd_release(listener_fd);
            return -(EWOULDBLOCK);
        }
        int reason = socket_wait_node(listener_fd->node, EPOLLIN, "socket_accept");
        if (reason != EOK) {
            fd_release(listener_fd);
            return -EINTR;
        }
    }

    if (!server_sock) {
        fd_release(listener_fd);
        return -ECONNABORTED;
    }

    // 创建节点
    vfs_node_t acceptFd = unix_socket_create_node(server_sock);
    if (!acceptFd) {
        fd_release(listener_fd);
        if (!unix_socket_requeue_pending_accept(listen_sock, server_sock)) {
            if (server_sock->peer) {
                server_sock->peer->peer        = NULL;
                server_sock->peer->established = false;
                socket_notify_sock(server_sock->peer, EPOLLERR | EPOLLHUP | EPOLLRDHUP);
            }
            unix_socket_free(server_sock);
        }
        return -ENOMEM;
    }
    socket_handle_t *accept_handle = acceptFd->handle;

    int ret           = -EMFILE;
    uint64_t i        = 0;
    fd_t *accepted_fd = NULL;
    with_fd_info_lock(current_task->fd_info, {
        for (i = 0; i < MAX_FD_NUM; i++) {
            if (current_task->fd_info->fds[i] == NULL)
                break;
        }

        if (i == MAX_FD_NUM)
            break;

        uint64_t accept_flags = O_RDWR;
        if ((flags & O_NONBLOCK) || listener_nonblock)
            accept_flags |= O_NONBLOCK;
        fd_t *new_fd = fd_create(acceptFd, accept_flags, !!(flags & O_CLOEXEC));
        if (!new_fd) {
            ret = -ENOMEM;
            break;
        }
        new_fd->fd                    = (int)i;
        current_task->fd_info->fds[i] = new_fd;
        accepted_fd                   = new_fd;
        procfs_on_open_file(current_task, i);
        ret = (int)i;
    });

    fd_release(listener_fd);

    if (ret < 0) {
        server_sock->node = NULL;
        if (accept_handle)
            accept_handle->sock = NULL;
        vfs_free(acceptFd);
        if (!unix_socket_requeue_pending_accept(listen_sock, server_sock)) {
            if (server_sock->peer) {
                server_sock->peer->peer        = NULL;
                server_sock->peer->established = false;
                socket_notify_sock(server_sock->peer, EPOLLERR | EPOLLHUP | EPOLLRDHUP);
            }
            unix_socket_free(server_sock);
        }
        return ret;
    }

    accept_handle->fd = accepted_fd;

    socket_notify_sock(server_sock, EPOLLOUT);
    if (server_sock->peer) {
        socket_notify_sock(server_sock->peer, EPOLLOUT);
    }

    if (addr) {
        struct sockaddr_un kaddr;
        socklen_t kaddrlen = 0;
        const char *name   = unix_socket_local_name(server_sock->peer);
        unix_socket_write_sockaddr(name, &kaddr, &kaddrlen);

        socklen_t user_len = *addrlen;
        size_t copy_len    = MIN((size_t)user_len, (size_t)kaddrlen);
        if (copy_len > 0)
            memcpy(addr, &kaddr, copy_len);
        *addrlen = kaddrlen;
    }

    return ret;
}

uint64_t socket_shutdown(uint64_t fd, uint64_t how) {
    if (fd >= MAX_FD_NUM || !current_task->fd_info->fds[fd])
        return -EBADF;
    if (how > SHUT_RDWR)
        return -EINVAL;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    if (unix_socket_is_connected_type(sock->type) && !sock->peer && !sock->established
        && sock->connMax == 0)
        return -ENOTCONN;

    if (how == SHUT_RD || how == SHUT_RDWR)
        sock->shut_rd = true;
    if (how == SHUT_WR || how == SHUT_RDWR)
        sock->shut_wr = true;

    socket_notify_sock(sock, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    if (sock->peer)
        socket_notify_sock(sock->peer, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);

    return 0;
}

int socket_connect(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen) {
    if (!addr)
        return -EFAULT;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    if (sock->connMax != 0)
        return -(ECONNREFUSED);

    if (sock->peer)
        return -(EISCONN);

    char *safe = unix_socket_addr_safe(addr, addrlen);
    if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK)
        return (uint64_t)safe;
    size_t safeLen        = strlen(safe);
    bool is_abstract      = (addr->sun_path[0] == '\0');
    socket_t *listen_sock = unix_socket_lookup_bound(safe, safeLen, sock, true);

    if (!listen_sock) {
        int ret = -ENOENT;
        if (!is_abstract) {
            vfs_node_t path_node = vfs_open(safe);
            if (path_node) {
                ret = -ECONNREFUSED;
                vfs_close(path_node);
            }
        }
        free(safe);
        return ret;
    }
    free(safe);

    if (listen_sock->type != sock->type) {
        unix_socket_release_lookup_ref(listen_sock);
        return -EPROTOTYPE;
    }

    while (true) {
        mutex_lock(&listen_sock->lock);
        if (listen_sock->closed || !listen_sock->connMax) {
            mutex_unlock(&listen_sock->lock);
            unix_socket_release_lookup_ref(listen_sock);
            return -ECONNREFUSED;
        }
        bool queue_available = listen_sock->connCurr < listen_sock->connMax;
        mutex_unlock(&listen_sock->lock);

        if (queue_available)
            break;

        if ((fd_get_flags(current_task->fd_info->fds[fd]) & O_NONBLOCK)) {
            unix_socket_release_lookup_ref(listen_sock);
            return -EAGAIN;
        }
        int reason = socket_wait_node(listen_sock->node, EPOLLOUT, "socket_connect");
        if (reason != EOK) {
            unix_socket_release_lookup_ref(listen_sock);
            return -EINTR;
        }
    }

    socket_t *server_sock = unix_socket_alloc();
    if (!server_sock) {
        unix_socket_release_lookup_ref(listen_sock);
        return -ENOMEM;
    }

    server_sock->domain   = listen_sock->domain;
    server_sock->type     = listen_sock->type;
    server_sock->protocol = listen_sock->protocol;
    server_sock->cred     = listen_sock->cred;
    server_sock->passcred = listen_sock->passcred;
    unix_socket_fill_cred_from_task(&sock->cred, current_task);
    unix_socket_snapshot_peer_cred(sock, &server_sock->cred);
    unix_socket_snapshot_peer_cred(server_sock, &sock->cred);
    if (listen_sock->bindAddr) {
        server_sock->filename = strdup(listen_sock->bindAddr);
        if (!server_sock->filename) {
            unix_socket_release_lookup_ref(listen_sock);
            unix_socket_free(server_sock);
            return -ENOMEM;
        }
    }

    server_sock->peer        = sock;
    sock->peer               = server_sock;
    server_sock->established = true;
    sock->established        = true;

    mutex_lock(&listen_sock->lock);
    if (listen_sock->closed || !listen_sock->connMax
        || listen_sock->connCurr >= listen_sock->connMax) {
        mutex_unlock(&listen_sock->lock);
        sock->peer        = NULL;
        sock->established = false;
        server_sock->peer = NULL;
        unix_socket_release_lookup_ref(listen_sock);
        unix_socket_free(server_sock);
        return -ECONNREFUSED;
    }
    if (!unix_socket_backlog_enqueue_tail_locked(listen_sock, server_sock)) {
        mutex_unlock(&listen_sock->lock);
        sock->peer        = NULL;
        sock->established = false;
        server_sock->peer = NULL;
        unix_socket_release_lookup_ref(listen_sock);
        unix_socket_free(server_sock);
        return -ENOMEM;
    }
    mutex_unlock(&listen_sock->lock);
    socket_notify_sock(listen_sock, EPOLLIN);
    socket_notify_sock(sock, EPOLLOUT);
    unix_socket_release_lookup_ref(listen_sock);

    return 0;
}

size_t unix_socket_sendto(
    uint64_t fd, uint8_t *in, size_t limit, int flags, struct sockaddr_un *addr, uint32_t len
) {
    socket_handle_t *handle            = current_task->fd_info->fds[fd]->node->handle;
    fd_t *caller_fd                    = current_task->fd_info->fds[fd];
    socket_t *sock                     = handle->sock;
    socket_t *peer                     = sock->peer;
    bool peer_needs_unref              = false;
    unix_socket_ancillary_t *ancillary = NULL;
    int ret                            = 0;

    if (!peer) {
        if (!unix_socket_is_dgram_type(sock->type) && sock->established) {
            if (!(flags & MSG_NOSIGNAL))
                task_commit_signal(current_task, SIGPIPE, NULL);
            return (size_t)-EPIPE;
        }

        if (addr && len) {
            char *safe = unix_socket_addr_safe(addr, len);
            if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK)
                return (uint64_t)safe;
            size_t safeLen      = strlen(safe);
            socket_t *peer_sock = unix_socket_lookup_bound(safe, safeLen, sock, true);
            free(safe);

            if (peer_sock) {
                peer             = peer_sock;
                peer_needs_unref = true;
                goto done;
            }
        }
        if (unix_socket_is_dgram_type(sock->type))
            return (size_t)-EDESTADDRREQ;
        return (size_t)-ENOTCONN;
    }

done:
    ret = unix_socket_maybe_add_passcred(peer, &ancillary);
    if (ret < 0) {
        if (peer_needs_unref)
            unix_socket_release_lookup_ref(peer);
        return (size_t)ret;
    }

    ret = (int)unix_socket_send_to_peer(sock, peer, in, limit, flags, caller_fd, &ancillary);
    if (ancillary)
        unix_socket_ancillary_free(ancillary);
    if (peer_needs_unref)
        unix_socket_release_lookup_ref(peer);
    return (size_t)ret;
}

size_t unix_socket_recvfrom(
    uint64_t fd, uint8_t *out, size_t limit, int flags, struct sockaddr_un *addr, uint32_t *len
) {
    fd_t *caller_fd         = current_task->fd_info->fds[fd];
    socket_handle_t *handle = caller_fd->node->handle;
    socket_t *sock          = handle->sock;

    if (!unix_socket_is_dgram_type(sock->type) && !sock->peer && !sock->established
        && sock->recv_pos == 0)
        return -(ENOTCONN);

    return unix_socket_recv_from_self(sock, sock->peer, out, limit, flags, caller_fd);
}

size_t unix_socket_sendmsg(uint64_t fd, const struct msghdr *msg, int flags) {
    fd_t *caller_fd                              = current_task->fd_info->fds[fd];
    socket_handle_t *handle                      = caller_fd->node->handle;
    socket_t *sock                               = handle->sock;
    socket_t *peer                               = sock->peer;
    bool peer_needs_unref                        = false;
    size_t total_len                             = 0;
    unix_socket_ancillary_t *ancillary           = NULL;
    int ancillary_ret                            = 0;
    size_t cnt                                   = 0;
    bool noblock                                 = false;
    unix_socket_ancillary_t *ancillary_to_attach = NULL;

    if (!peer) {
        if (!unix_socket_is_dgram_type(sock->type) && sock->established) {
            if (!(flags & MSG_NOSIGNAL))
                task_commit_signal(current_task, SIGPIPE, NULL);
            return (size_t)-EPIPE;
        }

        if (msg->msg_name && msg->msg_namelen) {
            char *safe = unix_socket_addr_safe(msg->msg_name, msg->msg_namelen);
            if (((uint64_t)safe & ERRNO_MASK) == ERRNO_MASK)
                return (uint64_t)safe;
            size_t safeLen      = strlen(safe);
            socket_t *peer_sock = unix_socket_lookup_bound(safe, safeLen, sock, true);
            free(safe);

            if (peer_sock) {
                peer             = peer_sock;
                peer_needs_unref = true;
                goto done;
            }
        }
        if (unix_socket_is_dgram_type(sock->type))
            return (size_t)-EDESTADDRREQ;
        return (size_t)-ENOTCONN;
    }

done:
    total_len     = unix_socket_iov_total_len(msg->msg_iov, msg->msg_iovlen);
    ancillary_ret = unix_socket_prepare_ancillary(msg, &ancillary);
    if (ancillary_ret < 0) {
        if (peer_needs_unref)
            unix_socket_release_lookup_ref(peer);
        return (size_t)ancillary_ret;
    }

    ancillary_ret = unix_socket_maybe_add_passcred(peer, &ancillary);
    if (ancillary_ret < 0) {
        unix_socket_ancillary_free(ancillary);
        if (peer_needs_unref)
            unix_socket_release_lookup_ref(peer);
        return (size_t)ancillary_ret;
    }

    if (ancillary && total_len == 0) {
        unix_socket_ancillary_free(ancillary);
        if (peer_needs_unref)
            unix_socket_release_lookup_ref(peer);
        return (size_t)-EINVAL;
    }

    cnt                 = 0;
    noblock             = !!(flags & MSG_DONTWAIT);
    ancillary_to_attach = ancillary;

    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct iovec *curr = &((struct iovec *)msg->msg_iov)[i];
        size_t sent        = 0;
        while (sent < curr->len) {
            const uint8_t *base = (const uint8_t *)curr->iov_base;
            size_t ret          = unix_socket_send_to_peer(
                sock,
                peer,
                base + sent,
                curr->len - sent,
                noblock ? (flags | MSG_DONTWAIT) : flags,
                caller_fd,
                &ancillary_to_attach
            );
            if ((int64_t)ret < 0) {
                if (peer_needs_unref)
                    unix_socket_release_lookup_ref(peer);
                if (ancillary_to_attach)
                    unix_socket_ancillary_free(ancillary_to_attach);
                if (cnt > 0)
                    return cnt;
                return ret;
            }
            if (ret == 0) {
                if (peer_needs_unref)
                    unix_socket_release_lookup_ref(peer);
                if (ancillary_to_attach)
                    unix_socket_ancillary_free(ancillary_to_attach);
                return cnt;
            }
            sent += ret;
            cnt += ret;
        }
    }

    if (peer_needs_unref)
        unix_socket_release_lookup_ref(peer);
    if (ancillary_to_attach)
        unix_socket_ancillary_free(ancillary_to_attach);
    return cnt;
}

size_t unix_socket_recvmsg(uint64_t fd, struct msghdr *msg, int flags) {
    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    fd_t *caller_fd         = current_task->fd_info->fds[fd];
    socket_t *sock          = handle->sock;
    if (!unix_socket_is_dgram_type(sock->type) && !sock->peer && !sock->established
        && sock->recv_pos == 0)
        return (size_t)-ENOTCONN;

    msg->msg_flags     = 0;
    uint64_t start_seq = 0;
    size_t cnt = unix_socket_recvmsg_from_self(sock, NULL, msg, flags, caller_fd, &start_seq);
    if ((int64_t)cnt < 0)
        return cnt;

    uint64_t end_seq                        = start_seq + cnt;
    unix_socket_ancillary_t *ancillary_list = NULL;
    mutex_lock(&sock->lock);
    int ancillary_ret =
        unix_socket_collect_ancillary_locked(sock, end_seq, !!(flags & MSG_PEEK), &ancillary_list);
    mutex_unlock(&sock->lock);
    if (ancillary_ret < 0)
        return (size_t)ancillary_ret;

    if (ancillary_list && msg->msg_control && msg->msg_controllen >= sizeof(struct cmsghdr)) {
        size_t controllen_used = 0;
        struct cmsghdr *cmsg   = CMSG_FIRSTHDR(msg);
        bool emitted_cred      = false;

        for (unix_socket_ancillary_t *anc = ancillary_list; anc != NULL; anc = anc->next) {
            if (anc->file_count > 0) {
                size_t space_left = msg->msg_controllen - controllen_used;
                if (cmsg && space_left >= CMSG_SPACE(anc->file_count * sizeof(int))) {
                    int *fds_out     = (int *)CMSG_DATA(cmsg);
                    size_t installed = unix_socket_install_pending_files(
                        anc->files, anc->file_count, fds_out, &msg->msg_flags, flags
                    );
                    anc->file_count = 0;

                    if (installed > 0) {
                        cmsg->cmsg_level = SOL_SOCKET;
                        cmsg->cmsg_type  = SCM_RIGHTS;
                        cmsg->cmsg_len   = CMSG_LEN(installed * sizeof(int));
                        controllen_used += CMSG_SPACE(installed * sizeof(int));
                        cmsg = CMSG_NXTHDR(msg, cmsg);
                    }
                } else {
                    msg->msg_flags |= MSG_CTRUNC;
                }
            }

            if (anc->has_cred) {
                if (emitted_cred)
                    continue;

                size_t space_left = msg->msg_controllen - controllen_used;
                if (cmsg && space_left >= CMSG_SPACE(sizeof(struct ucred))) {
                    cmsg->cmsg_level = SOL_SOCKET;
                    cmsg->cmsg_type  = SCM_CREDENTIALS;
                    cmsg->cmsg_len   = CMSG_LEN(sizeof(struct ucred));
                    memcpy(CMSG_DATA(cmsg), &anc->cred, sizeof(struct ucred));
                    controllen_used += CMSG_SPACE(sizeof(struct ucred));
                    cmsg         = CMSG_NXTHDR(msg, cmsg);
                    emitted_cred = true;
                } else {
                    msg->msg_flags |= MSG_CTRUNC;
                }
            }
        }

        msg->msg_controllen = controllen_used;
    } else {
        if (ancillary_list)
            msg->msg_flags |= MSG_CTRUNC;
        msg->msg_controllen = 0;
    }

    unix_socket_ancillary_free_list(ancillary_list);
    return cnt;
}

static int socket_poll(void *file, size_t events) {
    socket_handle_t *handler = file;
    if (!handler || !handler->sock)
        return EPOLLNVAL;
    socket_t *sock = handler->sock;
    int revents    = 0;

    if (sock->connMax > 0) {
        // listen 模式
        mutex_lock(&sock->lock);
        if (sock->connCurr > 0)
            revents |= (events & EPOLLIN) ? EPOLLIN : 0;
        if (sock->connCurr < sock->connMax)
            revents |= (events & EPOLLOUT) ? EPOLLOUT : 0;
        if (sock->closed)
            revents |= EPOLLERR | EPOLLHUP;
        mutex_unlock(&sock->lock);
    } else if (unix_socket_is_dgram_type(sock->type)) {
        mutex_lock(&sock->lock);
        if ((events & EPOLLOUT) && !sock->closed && !sock->shut_wr)
            revents |= EPOLLOUT;

        if ((events & EPOLLIN) && (sock->recv_pos > 0 || sock->ancillary_head != NULL))
            revents |= EPOLLIN;
        if (sock->closed || sock->shut_rd)
            revents |= EPOLLERR | EPOLLHUP;

        mutex_unlock(&sock->lock);
    } else {
        mutex_lock(&sock->lock);
        socket_t *peer = sock->peer;
        if (peer) {
            if (spin_trylock(peer->lock)) {
                if (peer->closed)
                    revents |= EPOLLHUP;
                if ((events & EPOLLRDHUP) && (peer->closed || peer->shut_wr))
                    revents |= EPOLLRDHUP;

                if ((events & EPOLLOUT) && !sock->shut_wr && !peer->closed
                    && unix_socket_recv_space_locked(peer) > 0)
                    revents |= EPOLLOUT;

                bool has_input = sock->recv_pos > 0 || sock->ancillary_head != NULL;
                if ((events & EPOLLIN)
                    && (has_input || sock->shut_rd || peer->shut_wr || peer->closed))
                    revents |= EPOLLIN;
                mutex_unlock(&peer->lock);
            } else {
                bool has_input = sock->recv_pos > 0 || sock->ancillary_head != NULL;
                if ((events & EPOLLIN) && has_input)
                    revents |= EPOLLIN;
                if (sock->closed || peer->closed)
                    revents |= EPOLLHUP | EPOLLERR;
            }
        } else {
            if ((events & EPOLLIN) && (sock->established || sock->ancillary_head != NULL))
                revents |= EPOLLIN;
            if ((events & EPOLLRDHUP) && sock->established)
                revents |= EPOLLRDHUP;
            if (sock->established || sock->closed || sock->shut_rd || sock->shut_wr)
                revents |= EPOLLHUP;
            if (sock->closed)
                revents |= EPOLLERR;
        }
        mutex_unlock(&sock->lock);
    }

    return revents;
}

static errno_t socket_ioctl(void *file, size_t cmd, void *arg) {
    socket_handle_t *handler = file;
    if (!handler || !handler->sock)
        return -EBADF;

    socket_t *sock = handler->sock;

    switch (cmd) {
    case FIONREAD:
        if (!arg)
            return -EFAULT;
        {
            int value   = (int)sock->recv_pos;
            *(int *)arg = value;
            return 0;
        }
    case FIONBIO:
        if (!arg)
            return -EFAULT;
        {
            const int enabled = (*(int *)arg != 0);
            if (handler->fd) {
                if (enabled)
                    handler->fd->flags |= O_NONBLOCK;
                else
                    handler->fd->flags &= ~O_NONBLOCK;
            }
            if (handler->node) {
                if (enabled)
                    handler->node->flags |= O_NONBLOCK;
                else
                    handler->node->flags &= ~O_NONBLOCK;
            }
        }
        return 0;
    default:
        return -ENOTTY;
    }
}

static bool socket_close(void *current) {
    socket_handle_t *handle = current;
    if (!handle)
        return true;

    socket_t *sock  = handle->sock;
    vfs_node_t node = handle->node;
    socket_t *peer  = NULL;

    if (!sock) {
        if (node)
            node->handle = NULL;
        free(handle);
        return true;
    }

    mutex_lock(&sock->lock);
    if (sock->refcount > 0)
        sock->refcount--;
    if (sock->refcount > 0) {
        mutex_unlock(&sock->lock);
        return true;
    }

    sock->closed = true;
    if (sock->connMax > 0 && sock->backlogCap > 0 && sock->connCurr > 0) {
        int pending = sock->connCurr;
        for (int i = 0; i < pending; i++) {
            int slot               = (sock->connHead + i) % sock->backlogCap;
            socket_t *pending_sock = sock->backlog[slot];
            sock->backlog[slot]    = NULL;
            if (!pending_sock)
                continue;

            socket_t *pending_peer    = pending_sock->peer;
            pending_sock->peer        = NULL;
            pending_sock->established = false;

            if (pending_peer && pending_peer->peer == pending_sock) {
                pending_peer->peer = NULL;
                socket_notify_sock(pending_peer, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);
            }
            socket_notify_sock(pending_sock, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);
            unix_socket_free(pending_sock);
        }
        sock->connCurr = 0;
        sock->connHead = 0;
    }

    if (sock->peer) {
        peer = sock->peer;
        unix_socket_snapshot_peer_cred(sock, &peer->cred);
        unix_socket_snapshot_peer_cred(peer, &sock->cred);
        sock->peer->peer = NULL; // 对端不再指向我
        sock->peer       = NULL;
    }
    mutex_unlock(&sock->lock);

    socket_notify_sock(sock, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);
    if (peer) {
        socket_notify_sock(peer, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);
    }

    if (node)
        node->handle = NULL;
    unix_socket_free(sock);
    free(handle);

    return true;
}

static size_t socket_read(void *file, void *buf, size_t offset, size_t limit) {
    socket_handle_t *handle = file;
    socket_t *sock          = handle->sock;

    if (!unix_socket_is_dgram_type(sock->type) && !sock->peer && !sock->established
        && sock->recv_pos == 0)
        return -(ENOTCONN);

    return unix_socket_recv_from_self(sock, sock->peer, buf, limit, 0, handle->fd);
}

static size_t socket_write(void *file, const void *buf, size_t offset, size_t limit) {
    socket_handle_t *handle            = file;
    socket_t *sock                     = handle->sock;
    unix_socket_ancillary_t *ancillary = NULL;
    int ret                            = 0;

    if (!sock->peer) {
        if (unix_socket_is_dgram_type(sock->type))
            return -(EDESTADDRREQ);
        if (!unix_socket_is_dgram_type(sock->type) && sock->established) {
            task_commit_signal(current_task, SIGPIPE, NULL);
            return -(EPIPE);
        }
        return -(ENOTCONN);
    }

    ret = unix_socket_maybe_add_passcred(sock->peer, &ancillary);
    if (ret < 0)
        return ret;

    ret = (int)unix_socket_send_to_peer(sock, sock->peer, buf, limit, 0, handle->fd, &ancillary);
    if (ancillary)
        unix_socket_ancillary_free(ancillary);
    return ret;
}

int unix_socket_pair(int domain, int type, int protocol, int *sv) {
    int sock_type = type & 0xF;
    if (!unix_socket_type_supported(sock_type)) {
        return -ESOCKTNOSUPPORT;
    }

    socket_t *sock1 = unix_socket_alloc();
    socket_t *sock2 = unix_socket_alloc();
    if (!sock1 || !sock2) {
        unix_socket_free(sock1);
        unix_socket_free(sock2);
        return -ENOMEM;
    }

    sock1->domain   = domain;
    sock1->type     = sock_type;
    sock1->protocol = protocol;

    sock2->domain   = domain;
    sock2->type     = sock_type;
    sock2->protocol = protocol;

    // 双向连接
    sock1->peer        = sock2;
    sock2->peer        = sock1;
    sock1->established = true;
    sock2->established = true;
    unix_socket_snapshot_peer_cred(sock1, &sock2->cred);
    unix_socket_snapshot_peer_cred(sock2, &sock1->cred);

    vfs_node_t node1 = unix_socket_create_node(sock1);
    vfs_node_t node2 = unix_socket_create_node(sock2);
    if (!node1 || !node2) {
        if (node1)
            vfs_free(node1);
        if (node2)
            vfs_free(node2);
        unix_socket_free(sock1);
        unix_socket_free(sock2);
        return -ENOMEM;
    }

    uint64_t flags = O_RDWR;
    if (type & O_NONBLOCK)
        flags |= O_NONBLOCK;

    int fd1 = -1, fd2 = -1;
    int ret = -EMFILE;
    with_fd_info_lock(current_task->fd_info, {
        for (int i = 0; i < MAX_FD_NUM; i++) {
            if (current_task->fd_info->fds[i] == NULL) {
                if (fd1 == -1)
                    fd1 = i;
                else {
                    fd2 = i;
                    break;
                }
            }
        }

        if (fd1 < 0 || fd2 < 0)
            break;

        fd_t *entry1 = fd_create(node1, O_RDWR | (flags & O_NONBLOCK), !!(type & O_CLOEXEC));
        fd_t *entry2 = fd_create(node2, O_RDWR | (flags & O_NONBLOCK), !!(type & O_CLOEXEC));
        if (!entry1 || !entry2) {
            if (entry1)
                fd_destroy(entry1);
            if (entry2)
                fd_destroy(entry2);
            ret = -ENOMEM;
            fd1 = fd2 = -1;
            break;
        }

        current_task->fd_info->fds[fd1] = entry1;
        current_task->fd_info->fds[fd2] = entry2;
        entry1->fd                      = fd1;
        entry2->fd                      = fd2;
        procfs_on_open_file(current_task, fd1);
        procfs_on_open_file(current_task, fd2);

        socket_handle_t *h1 = node1->handle;
        socket_handle_t *h2 = node2->handle;
        h1->fd              = entry1;
        h2->fd              = entry2;
        ret                 = 0;
    });

    if (ret < 0) {
        unix_socket_free(sock1);
        unix_socket_free(sock2);
        vfs_free(node1);
        vfs_free(node2);
        return ret;
    }

    sv[0] = fd1;
    sv[1] = fd2;

    return 0;
}

int unix_socket_getsockname(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen) {
    if (fd >= MAX_FD_NUM || !current_task->fd_info->fds[fd])
        return -(EBADF);

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    unix_socket_write_sockaddr(unix_socket_local_name(sock), addr, addrlen);

    return 0;
}

size_t unix_socket_getpeername(uint64_t fd, struct sockaddr_un *addr, socklen_t *len) {
    if (fd >= MAX_FD_NUM || !current_task->fd_info->fds[fd])
        return (size_t)-EBADF;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    if (!sock->peer)
        return -ENOTCONN;

    unix_socket_write_sockaddr(unix_socket_local_name(sock->peer), addr, len);

    return 0;
}

size_t
unix_socket_setsockopt(uint64_t fd, int level, int optname, const void *optval, socklen_t optlen) {
    if (level != SOL_SOCKET)
        return -ENOPROTOOPT;

    if (!current_task->fd_info->fds[fd])
        return (size_t)-EBADF;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    switch (optname) {
    case SO_REUSEADDR:
        if (optlen < sizeof(int))
            return -EINVAL;
        sock->reuseaddr = *(int *)optval;
        break;

    case SO_KEEPALIVE:
        if (optlen < sizeof(int))
            return -EINVAL;
        sock->keepalive = *(int *)optval;
        break;

    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (optlen < sizeof(struct timeval))
            return -EINVAL;
        memcpy(&sock->sndtimeo, optval, sizeof(struct timeval));
        break;

    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (optlen < sizeof(struct timeval))
            return -EINVAL;
        memcpy(&sock->rcvtimeo, optval, sizeof(struct timeval));
        break;

    case SO_BINDTODEVICE:
        if (optlen > IFNAMSIZ)
            return -EINVAL;
        strncpy(sock->bind_to_dev, optval, optlen);
        sock->bind_to_dev[IFNAMSIZ - 1] = '\0';
        break;

    case SO_LINGER:
        if (optlen < sizeof(struct linger))
            return -EINVAL;
        memcpy(&sock->linger_opt, optval, sizeof(struct linger));
        break;

    case SO_SNDBUF:
    case SO_RCVBUF:
        if (optlen < sizeof(int))
            return -EINVAL;
        {
            int new_size = *(int *)optval;
            if (new_size < BUFFER_SIZE)
                new_size = BUFFER_SIZE;

            mutex_lock(&sock->lock);
            void *newBuff = alloc_frames_bytes(new_size);
            if (!newBuff) {
                mutex_unlock(&sock->lock);
                return -ENOMEM;
            }
            size_t preserved = MIN((size_t)new_size, sock->recv_pos);
            if (preserved) {
                unix_socket_recv_copy_out_locked(sock, 0, newBuff, preserved);
            }
            free_frames_bytes(sock->recv_buff, sock->recv_size);
            sock->recv_buff = newBuff;
            sock->recv_size = new_size;
            sock->recv_head = 0;
            sock->recv_pos  = preserved;
            mutex_unlock(&sock->lock);
        }
        break;

    case SO_PASSCRED:
        if (optlen < sizeof(int))
            return -EINVAL;
        sock->passcred = *(int *)optval;
        break;

    case SO_PEERCRED:
        return -ENOPROTOOPT; // 只读

    default:
        logkf("Unsupported setsockopt, optname = %d\n", optname);
        return -ENOPROTOOPT;
    }

    return 0;
}

size_t
unix_socket_getsockopt(uint64_t fd, int level, int optname, void *optval, socklen_t *optlen) {
    if (level != SOL_SOCKET)
        return -ENOPROTOOPT;

    if (!current_task->fd_info->fds[fd])
        return (size_t)-EBADF;

    socket_handle_t *handle = current_task->fd_info->fds[fd]->node->handle;
    socket_t *sock          = handle->sock;

    switch (optname) {
    case SO_ERROR:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = 0;
        *optlen        = sizeof(int);
        break;

    case SO_REUSEADDR:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->reuseaddr;
        *optlen        = sizeof(int);
        break;

    case SO_KEEPALIVE:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->keepalive;
        *optlen        = sizeof(int);
        break;

    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (*optlen < sizeof(struct timeval))
            return -EINVAL;
        memcpy(optval, &sock->sndtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;

    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (*optlen < sizeof(struct timeval))
            return -EINVAL;
        memcpy(optval, &sock->rcvtimeo, sizeof(struct timeval));
        *optlen = sizeof(struct timeval);
        break;

    case SO_BINDTODEVICE:
        if (*optlen < IFNAMSIZ)
            return -EINVAL;
        strncpy(optval, sock->bind_to_dev, IFNAMSIZ);
        *optlen = strlen(sock->bind_to_dev) + 1;
        break;

    case SO_PROTOCOL:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->protocol;
        *optlen        = sizeof(int);
        break;

    case SO_DOMAIN:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->domain;
        *optlen        = sizeof(int);
        break;

    case SO_LINGER:
        if (*optlen < sizeof(struct linger))
            return -EINVAL;
        memcpy(optval, &sock->linger_opt, sizeof(struct linger));
        *optlen = sizeof(struct linger);
        break;

    case SO_SNDBUF:
    case SO_RCVBUF:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->recv_size;
        *optlen        = sizeof(int);
        break;

    case SO_PASSCRED:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->passcred;
        *optlen        = sizeof(int);
        break;

    case SO_PEERCRED: {
        struct ucred peer_cred = { 0 };
        if (!unix_socket_get_peer_cred(sock, &peer_cred))
            return -ENOTCONN;
        if (*optlen < sizeof(struct ucred))
            return -EINVAL;
        memcpy(optval, &peer_cred, sizeof(struct ucred));
        *optlen = sizeof(struct ucred);
    } break;

    case SO_ACCEPTCONN:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = (sock->connMax > 0) ? 1 : 0;
        *optlen        = sizeof(int);
        break;

    case SO_TYPE:
        if (*optlen < sizeof(int))
            return -EINVAL;
        *(int *)optval = sock->type;
        *optlen        = sizeof(int);
        break;

    default:
        logkf("Unsupported getsockopt, optname = %d\n", optname);
        return -ENOPROTOOPT;
    }

    return 0;
}

socket_op_t socket_ops = {
    .shutdown    = socket_shutdown,
    .accept      = socket_accept,
    .listen      = socket_listen,
    .getsockname = unix_socket_getsockname,
    .bind        = socket_bind,
    .connect     = socket_connect,
    .sendto      = unix_socket_sendto,
    .recvfrom    = unix_socket_recvfrom,
    .sendmsg     = unix_socket_sendmsg,
    .recvmsg     = unix_socket_recvmsg,
    .getpeername = unix_socket_getpeername,
    .getsockopt  = unix_socket_getsockopt,
    .setsockopt  = unix_socket_setsockopt,
};

static void socketfs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_socket;
}

static errno_t socketfs_stat(void *file, vfs_node_t node) {
    socket_handle_t *handle = file;
    if (handle && handle->sock)
        node->size = handle->sock->recv_pos;
    return EOK;
}

static struct vfs_callback socketfs_callbacks = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = socketfs_open,
    .close    = socket_close,
    .read     = socket_read,
    .write    = socket_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .stat     = socketfs_stat,
    .ioctl    = socket_ioctl,
    .dup      = (vfs_dup_t)dummy,
    .poll     = socket_poll,
    .map      = (vfs_mapfile_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .free     = (vfs_free_t)dummy,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void socketfs_init() {
    if (socketfs_root)
        return;

    unix_socket_fsid = vfs_regist("sockfs", &socketfs_callbacks, 0x534F434B, FS_VIRTUAL_FLAGS);
    if (unix_socket_fsid < 0)
        return;

    socketfs_root         = vfs_node_alloc(rootdir, ".sockfs");
    socketfs_root->type   = file_dir;
    socketfs_root->fsid   = unix_socket_fsid;
    socketfs_root->handle = calloc(1, sizeof(socket_handle_t));

    unix_socket_list_lock = SPIN_INIT;
    unix_socket_bind_lock = SPIN_INIT;
    memset(&first_unix_socket, 0, sizeof(socket_t));
    unix_socket_list_tail = &first_unix_socket;
    unix_socket_bind_map  = HASHMAP_INIT;

    regist_socket(AF_UNIX, NULL, socket_socket, unix_socket_pair);
    netlink_init();
}
