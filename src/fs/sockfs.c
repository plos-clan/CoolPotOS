#define ALL_IMPLEMENTATION
#include "fs/sockfs.h"
#include "errno.h"
#include "fs/tmpfs.h"
#include "krlibc.h"
#include "task/scheduler.h"
#include "term/klog.h"

vfs_node_t sockfs_root = NULL;
int sockfs_id          = 0;
static int sockfd_id   = 0;

typedef struct sockfs_bind_entry {
    struct sockfs_bind_entry *next;
    socket_info_t *info;
    bool is_abstract;
    char key[UNIX_PATH_MAX];
} sockfs_bind_entry_t;

static spin_t sockfs_bind_lock         = SPIN_INIT;
static sockfs_bind_entry_t *sock_binds = NULL;

static void sockfs_trace_key(const char *op, const char *key, bool hit) {
    if (key == NULL || strstr(key, "X11-unix/X") == NULL) {
        return;
    }
    const tcb_t current = get_current_task();
    if (current == NULL || current->process == NULL || current->process->name == NULL) {
        return;
    }
    if (!strstr(current->process->name, "xinit") && !strstr(current->process->name, "Xorg")) {
        return;
    }
    logkf(
        "[sockfs-dbg] proc=%s pid=%d op=%s key=%s hit=%d\n",
        current->process->name,
        current->process->pid,
        op,
        key,
        hit
    );
}

// --- Ring buffer operations ---

void ringbuf_init(sock_ringbuf_t *rb, size_t capacity) {
    rb->buf      = calloc(1, capacity);
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
}

size_t ringbuf_read(sock_ringbuf_t *rb, void *dst, size_t len) {
    if (rb->count == 0 || len == 0)
        return 0;
    size_t to_read = MIN(len, rb->count);
    char *d        = (char *)dst;
    for (size_t i = 0; i < to_read; i++) {
        d[i]     = rb->buf[rb->head];
        rb->head = (rb->head + 1) % rb->capacity;
    }
    rb->count -= to_read;
    return to_read;
}

size_t ringbuf_write(sock_ringbuf_t *rb, const void *src, size_t len) {
    if (len == 0)
        return 0;
    size_t avail    = rb->capacity - rb->count;
    size_t to_write = MIN(len, avail);
    if (to_write == 0)
        return 0;
    const char *s = (const char *)src;
    for (size_t i = 0; i < to_write; i++) {
        rb->buf[rb->tail] = s[i];
        rb->tail          = (rb->tail + 1) % rb->capacity;
    }
    rb->count += to_write;
    return to_write;
}

size_t ringbuf_peek(sock_ringbuf_t *rb, void *dst, size_t len) {
    if (rb->count == 0 || len == 0)
        return 0;
    size_t to_read = MIN(len, rb->count);
    char *d        = (char *)dst;
    size_t h       = rb->head;
    for (size_t i = 0; i < to_read; i++) {
        d[i] = rb->buf[h];
        h    = (h + 1) % rb->capacity;
    }
    return to_read;
}

void ringbuf_destroy(sock_ringbuf_t *rb) {
    if (rb->buf) {
        free(rb->buf);
        rb->buf = NULL;
    }
    rb->capacity = rb->head = rb->tail = rb->count = 0;
}

static void sockfs_release_node(vfs_node_t node) {
    if (node == NULL) {
        return;
    }
    if (node->parent) {
        list_delete(node->parent->child, node);
    }
    node->child = list_free(node->child);
    free(node->name);
    if (node->linkto_path) {
        free(node->linkto_path);
    }
    free(node);
}

static errno_t sockfs_build_key(
    const struct sockaddr_un *sun, uint64_t addrlen, char key[UNIX_PATH_MAX], bool *is_abstract
) {
    if (sun == NULL || key == NULL) {
        return -EINVAL;
    }

    if (sun->sun_family != AF_UNIX) {
        return -EAFNOSUPPORT;
    }

    if (sun->sun_path[0] == '\0') {
        const size_t max_name = UNIX_PATH_MAX - 2;
        size_t raw_len        = 0;

        if (addrlen > sizeof(sun->sun_family) + 1) {
            raw_len = addrlen - sizeof(sun->sun_family) - 1;
            if (raw_len > max_name) {
                raw_len = max_name;
            }
        } else {
            raw_len = strnlen(sun->sun_path + 1, max_name);
        }

        if (raw_len == 0) {
            return -EINVAL;
        }

        key[0] = '@';
        memcpy(key + 1, sun->sun_path + 1, raw_len);
        key[raw_len + 1] = '\0';
        if (is_abstract) {
            *is_abstract = true;
        }
        return EOK;
    }

    char *path = vfs_cwd_path_build((char *)sun->sun_path);
    if (path == NULL) {
        return -ENOMEM;
    }

    const size_t len = strlen(path);
    if (len >= UNIX_PATH_MAX) {
        free(path);
        return -ENAMETOOLONG;
    }

    memcpy(key, path, len + 1);
    free(path);
    if (is_abstract) {
        *is_abstract = false;
    }
    return EOK;
}

static void sockfs_destroy_pending_server(socket_info_t *server_info) {
    if (server_info == NULL) {
        return;
    }

    if (server_info->peer) {
        socket_info_t *peer = server_info->peer;
        spin_lock(peer->lock);
        peer->peer_cred     = server_info->cred;
        peer->has_peer_cred = true;
        if (peer->peer == server_info) {
            peer->peer = NULL;
        }
        spin_unlock(peer->lock);
        sockfs_notify(peer, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
        server_info->peer = NULL;
    }

    ringbuf_destroy(&server_info->recv_buf);
    free(server_info);
}

static void sockfs_destroy_info(socket_info_t *info) {
    if (info == NULL) {
        return;
    }

    if (info->pending_queue) {
        for (int i = 0; i < info->pending_count; i++) {
            sockfs_destroy_pending_server(info->pending_queue[i]);
        }
        free(info->pending_queue);
        info->pending_queue = NULL;
    }

    sockfs_unbind_endpoint(info);
    ringbuf_destroy(&info->recv_buf);
    free(info);
}

static void sockfs_leave(socket_specific_t *spec) {
    if (spec == NULL || spec->info == NULL) {
        return;
    }

    socket_info_t *info = spec->info;
    bool free_spec      = false;
    bool free_info      = false;
    vfs_node_t node     = spec->node;

    spin_lock(info->lock);
    spec->active--;
    info->active--;
    free_spec = spec->free_pending && spec->active == 0;
    free_info = info->free_pending && info->active == 0;
    spin_unlock(info->lock);

    if (free_info) {
        sockfs_destroy_info(info);
    }

    if (free_spec) {
        if (node) {
            node->handle = NULL;
        }
        free(spec);
        if (node && node->refcount == 0) {
            sockfs_release_node(node);
        }
    }
}

void sockfs_notify(socket_info_t *info, uint32_t events) {
    if (info == NULL || events == 0) {
        return;
    }

    if (info->endpoint_node) {
        vfs_poll_notify(info->endpoint_node, events);
    }
}

errno_t sockfs_bind_endpoint(socket_info_t *info, const struct sockaddr_un *sun, uint64_t addrlen) {
    if (info == NULL || sun == NULL) {
        return -EINVAL;
    }
    if (info->bound_registered) {
        return -EINVAL;
    }

    char key[UNIX_PATH_MAX];
    bool is_abstract = false;
    errno_t ret      = sockfs_build_key(sun, addrlen, key, &is_abstract);
    if (ret < 0) {
        return ret;
    }

    spin_lock(sockfs_bind_lock);
    for (sockfs_bind_entry_t *it = sock_binds; it; it = it->next) {
        if (strcmp(it->key, key) == 0) {
            spin_unlock(sockfs_bind_lock);
            return -EADDRINUSE;
        }
    }
    spin_unlock(sockfs_bind_lock);

    vfs_node_t node = NULL;
    if (!is_abstract) {
        vfs_node_t existing = vfs_open(key);
        if (existing != NULL) {
            vfs_close(existing);
            return -EADDRINUSE;
        }

        ret = vfs_mknod(key, S_IFSOCK | 0666, 0);
        if (ret < 0) {
            return ret;
        }

        node = vfs_open(key);
        if (node == NULL) {
            return -ENOENT;
        }
    }

    sockfs_bind_entry_t *entry = calloc(1, sizeof(sockfs_bind_entry_t));
    if (entry == NULL) {
        if (node) {
            vfs_delete(node);
            vfs_close(node);
        }
        return -ENOMEM;
    }

    entry->info        = info;
    entry->is_abstract = is_abstract;
    memcpy(entry->key, key, sizeof(entry->key));

    spin_lock(sockfs_bind_lock);
    for (sockfs_bind_entry_t *it = sock_binds; it; it = it->next) {
        if (strcmp(it->key, key) == 0) {
            spin_unlock(sockfs_bind_lock);
            free(entry);
            if (node) {
                vfs_delete(node);
                vfs_close(node);
            }
            return -EADDRINUSE;
        }
    }
    entry->next       = sock_binds;
    sock_binds        = entry;
    info->is_bound    = true;
    info->bound_abstract = is_abstract;
    info->bound_registered = true;
    info->bound_node      = node;
    memcpy(info->bound_path, key, sizeof(info->bound_path));
    if (info->state == SS_UNCONNECTED) {
        info->state = SS_BOUND;
    }
    spin_unlock(sockfs_bind_lock);
    sockfs_trace_key("bind", key, true);

    return EOK;
}

errno_t sockfs_lookup_bound(
    const struct sockaddr_un *sun, uint64_t addrlen, socket_info_t **out_info, bool *path_exists
) {
    if (out_info == NULL) {
        return -EINVAL;
    }

    *out_info = NULL;
    if (path_exists) {
        *path_exists = false;
    }

    char key[UNIX_PATH_MAX];
    bool is_abstract = false;
    errno_t ret      = sockfs_build_key(sun, addrlen, key, &is_abstract);
    if (ret < 0) {
        return ret;
    }

    spin_lock(sockfs_bind_lock);
    for (sockfs_bind_entry_t *it = sock_binds; it; it = it->next) {
        if (strcmp(it->key, key) == 0) {
            *out_info = it->info;
            if (path_exists) {
                *path_exists = true;
            }
            spin_unlock(sockfs_bind_lock);
            sockfs_trace_key("lookup", key, true);
            return EOK;
        }
    }
    spin_unlock(sockfs_bind_lock);

    if (!is_abstract && path_exists) {
        vfs_node_t node = vfs_open(key);
        if (node) {
            *path_exists = true;
            vfs_close(node);
        }
    }

    sockfs_trace_key("lookup", key, false);

    return EOK;
}

void sockfs_unbind_endpoint(socket_info_t *info) {
    if (info == NULL || !info->bound_registered) {
        return;
    }

    char key[UNIX_PATH_MAX];
    memcpy(key, info->bound_path, sizeof(key));
    vfs_node_t node = NULL;

    spin_lock(sockfs_bind_lock);
    sockfs_bind_entry_t **it = &sock_binds;
    while (*it) {
        if ((*it)->info == info) {
            sockfs_bind_entry_t *entry = *it;
            *it                        = entry->next;
            free(entry);
            break;
        }
        it = &(*it)->next;
    }
    info->bound_registered = false;
    node                   = info->bound_node;
    info->bound_node       = NULL;
    spin_unlock(sockfs_bind_lock);

    sockfs_trace_key("unbind", key, false);
    if (node) {
        vfs_delete(node);
        vfs_close(node);
    }
}

void sockfs_fill_sockaddr(const socket_info_t *info, struct sockaddr_un *sun, socklen_t *addrlen) {
    if (sun == NULL || addrlen == NULL) {
        return;
    }

    struct sockaddr_un out;
    memset(&out, 0, sizeof(out));
    out.sun_family = AF_UNIX;
    socklen_t actual_len = sizeof(out.sun_family);
    if (info && info->is_bound) {
        if (info->bound_abstract) {
            const size_t raw_len = strnlen(info->bound_path + 1, UNIX_PATH_MAX - 2);
            out.sun_path[0] = '\0';
            strncpy(out.sun_path + 1, info->bound_path + 1, UNIX_PATH_MAX - 2);
            actual_len += (socklen_t)(1 + raw_len);
        } else {
            const size_t raw_len = strnlen(info->bound_path, UNIX_PATH_MAX - 1);
            strncpy(out.sun_path, info->bound_path, UNIX_PATH_MAX - 1);
            actual_len += (socklen_t)(raw_len + 1);
        }
    }

    const size_t copy_len = MIN((size_t)*addrlen, (size_t)actual_len);
    if (copy_len > 0)
        memcpy(sun, &out, copy_len);
    *addrlen = actual_len;
}

// --- sockfs VFS callbacks ---

static void sockfs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_socket;
}

static size_t sockfs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec)
        return (size_t)-1;
    socket_info_t *info = spec->info;
    if (!info)
        return (size_t)-1;
    if (info->shut_rd)
        return 0;

    spin_lock(info->lock);
    spec->active++;
    info->active++;
    spin_unlock(info->lock);

    size_t ret = (size_t)-1;
    for (;;) {
        spin_lock(info->lock);
        if (info->recv_buf.count > 0) {
            size_t to_read = MIN(size, info->recv_buf.count);
            ringbuf_read(&info->recv_buf, addr, to_read);
            if (spec->node)
                spec->node->size = info->recv_buf.count;
            socket_info_t *peer = info->peer;
            spin_unlock(info->lock);
            sockfs_notify(peer, EPOLLOUT);
            ret = to_read;
            goto out;
        }
        // No data: check if peer is gone
        if (info->type == SOCK_STREAM && info->peer == NULL && info->state != SS_LISTENING
            && info->state != SS_UNCONNECTED) {
            spin_unlock(info->lock);
            ret = 0; // EOF
            goto out;
        }
        // SOCK_DGRAM with no peer and no data
        if (info->type == SOCK_DGRAM && info->peer == NULL && !info->is_bound) {
            spin_unlock(info->lock);
            ret = 0;
            goto out;
        }
        spin_unlock(info->lock);
        scheduler_yield();
    }

out:
    sockfs_leave(spec);
    return ret;
}

static size_t sockfs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec)
        return (size_t)-1;
    socket_info_t *info = spec->info;
    if (!info)
        return (size_t)-1;
    if (info->shut_wr)
        return (size_t)-1;

    // Determine target buffer: for SOCK_STREAM write to peer's recv_buf
    socket_info_t *target = info->peer;
    if (info->type == SOCK_STREAM && target == NULL)
        return (size_t)-1;
    if (info->type == SOCK_DGRAM && target == NULL)
        return (size_t)-1;

    spin_lock(info->lock);
    spec->active++;
    info->active++;
    spin_unlock(info->lock);

    const uint8_t *src = (const uint8_t *)addr;
    size_t total       = 0;
    size_t remaining   = size;

    while (remaining > 0) {
        spin_lock(target->lock);
        size_t avail = target->recv_buf.capacity - target->recv_buf.count;
        if (avail > 0) {
            size_t to_write = MIN(remaining, avail);
            ringbuf_write(&target->recv_buf, src + total, to_write);
            total += to_write;
            remaining -= to_write;
            spin_unlock(target->lock);
            sockfs_notify(target, EPOLLIN);
        } else {
            spin_unlock(target->lock);
            if (info->peer == NULL)
                break; // peer disconnected
            scheduler_yield();
        }
    }

    sockfs_leave(spec);
    return total > 0 ? total : (size_t)-1;
}

static bool sockfs_close(void *current) {
    socket_specific_t *spec = (socket_specific_t *)current;
    if (!spec)
        return true;
    socket_info_t *info = spec->info;
    if (!info) {
        if (spec->node) {
            spec->node->handle = NULL;
        }
        free(spec);
        return true;
    }

    socket_info_t *peer = NULL;
    bool free_info      = false;
    bool free_spec      = false;
    const bool last_node_ref = spec->node && spec->node->refcount == 0;

    spin_lock(info->lock);
    if (info->refcount > 0) {
        info->refcount--;
    }

    if (info->refcount == 0) {
        info->closed = true;
        peer = info->peer;
        if (peer) {
            spin_lock(peer->lock);
            peer->peer_cred     = info->cred;
            peer->has_peer_cred = true;
            if (peer->peer == info) {
                peer->peer = NULL;
            }
            spin_unlock(peer->lock);
            info->peer = NULL;
        }

        if (info->active == 0) {
            free_info = true;
        } else {
            info->free_pending = true;
        }
    }

    if (last_node_ref) {
        info->endpoint_node = NULL;
        if (spec->active == 0) {
            free_spec = true;
        } else {
            spec->free_pending = true;
        }
    }
    spin_unlock(info->lock);

    if (peer) {
        sockfs_notify(peer, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    }

    if (free_info) {
        sockfs_destroy_info(info);
    }
    if (free_spec) {
        if (spec->node) {
            spec->node->handle = NULL;
        }
        free(spec);
    }
    return true;
}

static int sockfs_poll(void *file, size_t events) {
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec)
        return 0;
    socket_info_t *info = spec->info;
    if (!info)
        return 0;

    int out = 0;
    spin_lock(info->lock);
    if (info->state == SS_LISTENING) {
        if ((events & EPOLLIN) && info->pending_count > 0)
            out |= EPOLLIN;
        if ((events & EPOLLOUT) && info->pending_count < info->backlog)
            out |= EPOLLOUT;
        spin_unlock(info->lock);
        return out;
    }
    if (info->type == SOCK_DGRAM) {
        if ((events & EPOLLOUT) && !info->closed && !info->shut_wr && info->recv_buf.count < info->recv_buf.capacity)
            out |= EPOLLOUT;
        if ((events & EPOLLIN) && info->recv_buf.count > 0)
            out |= EPOLLIN;
        if (info->closed || info->shut_rd)
            out |= EPOLLERR | EPOLLHUP;
        spin_unlock(info->lock);
        return out;
    }

    socket_info_t *peer = info->peer;
    if (peer) {
        if (peer->closed)
            out |= EPOLLHUP;
        if ((events & EPOLLRDHUP) && (peer->closed || peer->shut_wr))
            out |= EPOLLRDHUP;
        if ((events & EPOLLOUT) && !info->shut_wr && !peer->closed && !peer->shut_rd
            && peer->recv_buf.count < peer->recv_buf.capacity)
            out |= EPOLLOUT;
        if ((events & EPOLLIN)
            && (info->recv_buf.count > 0 || info->shut_rd || peer->shut_wr || peer->closed))
            out |= EPOLLIN;
    } else if (info->state == SS_CONNECTED || info->closed || info->shut_rd || info->shut_wr) {
        if (events & EPOLLIN)
            out |= EPOLLIN;
        if (events & EPOLLRDHUP)
            out |= EPOLLRDHUP;
        out |= EPOLLHUP;
        if (info->closed)
            out |= EPOLLERR;
    }
    spin_unlock(info->lock);
    return out;
}

static errno_t sockfs_stat(void *file, vfs_node_t node) {
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec)
        return EOK;
    socket_info_t *info = spec->info;
    if (!info)
        return EOK;
    node->size = info->recv_buf.count;
    return EOK;
}

static errno_t sockfs_free(void *handle) {
    (void)handle;
    return EOK;
}

static int sockfs_mount(const char *handle, vfs_node_t node, void *data) {
    if (sockfs_root != NULL) {
        return -EBUSY;
    }
    node->fsid   = sockfs_id;
    sockfs_root  = node;
    node->handle = calloc(1, sizeof(socket_specific_t));
    return EOK;
}

static struct vfs_callback sockfs_callbacks = {
    .mount    = sockfs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)sockfs_open,
    .close    = sockfs_close,
    .read     = sockfs_read,
    .write    = sockfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = sockfs_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .poll     = sockfs_poll,
    .dup      = (vfs_dup_t)dummy,
    .free     = sockfs_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void sockfs_regist() {
    sockfs_id = vfs_regist("sockfs", &sockfs_callbacks, 0x534F434B, FS_VIRTUAL_FLAGS);
    if (sockfs_id == -EINVAL) {
        kerror("sockfs regist error.");
    }
    // Auto-create internal root node (not visible in VFS tree)
    sockfs_root         = vfs_node_alloc(rootdir, ".sockfs");
    sockfs_root->type   = file_dir;
    sockfs_root->fsid   = sockfs_id;
    sockfs_root->handle = calloc(1, sizeof(socket_specific_t));
}

// Helper: allocate a new socket_info + two VFS nodes + fd
vfs_node_t sockfs_create_node(socket_info_t *info) {
    if (!sockfs_root)
        return NULL;
    char buf[16];
    sprintf(buf, "sock%d", sockfd_id++);
    vfs_node_t node = vfs_node_alloc(sockfs_root, buf);
    node->type      = file_socket;
    node->fsid      = sockfs_id;
    node->mode      = 0700;

    socket_specific_t *spec = calloc(1, sizeof(socket_specific_t));
    spec->info              = info;
    spec->node              = node;
    spec->active            = 0;
    spec->free_pending      = false;
    node->handle            = spec;
    info->endpoint_node     = node;

    return node;
}
