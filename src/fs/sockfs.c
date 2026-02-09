#define ALL_IMPLEMENTATION
#include "fs/sockfs.h"
#include "errno.h"
#include "krlibc.h"
#include "task/scheduler.h"
#include "term/klog.h"

vfs_node_t sockfs_root = NULL;
int        sockfs_id   = 0;
static int sockfd_id   = 0;

// --- Ring buffer operations ---

void ringbuf_init(sock_ringbuf_t *rb, size_t capacity) {
    rb->buf      = calloc(1, capacity);
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
}

size_t ringbuf_read(sock_ringbuf_t *rb, void *dst, size_t len) {
    if (rb->count == 0 || len == 0) return 0;
    size_t to_read = MIN(len, rb->count);
    char  *d       = (char *)dst;
    for (size_t i = 0; i < to_read; i++) {
        d[i]     = rb->buf[rb->head];
        rb->head = (rb->head + 1) % rb->capacity;
    }
    rb->count -= to_read;
    return to_read;
}

size_t ringbuf_write(sock_ringbuf_t *rb, const void *src, size_t len) {
    if (len == 0) return 0;
    size_t avail    = rb->capacity - rb->count;
    size_t to_write = MIN(len, avail);
    if (to_write == 0) return 0;
    const char *s = (const char *)src;
    for (size_t i = 0; i < to_write; i++) {
        rb->buf[rb->tail] = s[i];
        rb->tail           = (rb->tail + 1) % rb->capacity;
    }
    rb->count += to_write;
    return to_write;
}

size_t ringbuf_peek(sock_ringbuf_t *rb, void *dst, size_t len) {
    if (rb->count == 0 || len == 0) return 0;
    size_t to_read = MIN(len, rb->count);
    char  *d       = (char *)dst;
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

// --- sockfs VFS callbacks ---

static void sockfs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_socket;
}

static size_t sockfs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec) return (size_t)-1;
    socket_info_t *info = spec->info;
    if (!info) return (size_t)-1;
    if (spec->shut_rd) return 0;

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
            if (spec->node) spec->node->size = info->recv_buf.count;
            spin_unlock(info->lock);
            ret = to_read;
            goto out;
        }
        // No data: check if peer is gone
        if (info->type == SOCK_STREAM && info->peer == NULL &&
            info->state != SS_LISTENING && info->state != SS_UNCONNECTED) {
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
    spin_lock(info->lock);
    spec->active--;
    info->active--;
    spin_unlock(info->lock);
    return ret;
}

static size_t sockfs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec) return (size_t)-1;
    socket_info_t *info = spec->info;
    if (!info) return (size_t)-1;
    if (spec->shut_wr) return (size_t)-1;

    // Determine target buffer: for SOCK_STREAM write to peer's recv_buf
    socket_info_t *target = info->peer;
    if (info->type == SOCK_STREAM && target == NULL) return (size_t)-1;
    if (info->type == SOCK_DGRAM && target == NULL) return (size_t)-1;

    spin_lock(info->lock);
    spec->active++;
    info->active++;
    spin_unlock(info->lock);

    const uint8_t *src       = (const uint8_t *)addr;
    size_t         total     = 0;
    size_t         remaining = size;

    while (remaining > 0) {
        spin_lock(target->lock);
        size_t avail = target->recv_buf.capacity - target->recv_buf.count;
        if (avail > 0) {
            size_t to_write = MIN(remaining, avail);
            ringbuf_write(&target->recv_buf, src + total, to_write);
            total     += to_write;
            remaining -= to_write;
            spin_unlock(target->lock);
        } else {
            spin_unlock(target->lock);
            if (info->peer == NULL) break; // peer disconnected
            scheduler_yield();
        }
    }

    spin_lock(info->lock);
    spec->active--;
    info->active--;
    spin_unlock(info->lock);
    return total > 0 ? total : (size_t)-1;
}

static bool sockfs_close(void *current) {
    socket_specific_t *spec = (socket_specific_t *)current;
    if (!spec) return true;
    socket_info_t *info = spec->info;
    if (!info) {
        free(spec);
        return true;
    }

    spin_lock(info->lock);
    info->refcount--;

    bool do_free_info = false;

    // Disconnect peer
    if (info->peer) {
        socket_info_t *peer = info->peer;
        spin_lock(peer->lock);
        if (peer->peer == info) peer->peer = NULL;
        spin_unlock(peer->lock);
        info->peer = NULL;
    }

    if (info->refcount <= 0) {
        if (info->active == 0) {
            do_free_info = true;
        } else {
            info->free_pending = true;
        }
    }
    spin_unlock(info->lock);

    if (spec->node) spec->node->handle = NULL;
    free(spec);

    if (do_free_info) {
        ringbuf_destroy(&info->recv_buf);
        if (info->pending_queue) free(info->pending_queue);
        // Clean up bound VFS node
        if (info->is_bound && info->bound_node) {
            vfs_node_t bn = info->bound_node;
            bn->handle    = NULL;
            if (bn->parent) list_delete(bn->parent->child, bn);
            vfs_free(bn);
        }
        free(info);
    }
    return true;
}

static int sockfs_poll(void *file, size_t events) {
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec) return 0;
    socket_info_t *info = spec->info;
    if (!info) return 0;

    int out = 0;
    spin_lock(info->lock);
    if (events & EPOLLIN) {
        if (info->recv_buf.count > 0) out |= EPOLLIN;
        if (info->type == SOCK_STREAM && info->peer == NULL &&
            info->state == SS_CONNECTED)
            out |= EPOLLHUP;
        if (info->state == SS_LISTENING && info->pending_count > 0)
            out |= EPOLLIN;
    }
    if (events & EPOLLOUT) {
        if (info->peer) {
            socket_info_t *peer = info->peer;
            if (peer->recv_buf.count < peer->recv_buf.capacity)
                out |= EPOLLOUT;
        }
        if (info->type == SOCK_STREAM && info->peer == NULL &&
            info->state == SS_CONNECTED)
            out |= EPOLLHUP;
    }
    spin_unlock(info->lock);
    return out;
}

static errno_t sockfs_stat(void *file, vfs_node_t node) {
    socket_specific_t *spec = (socket_specific_t *)file;
    if (!spec) return EOK;
    socket_info_t *info = spec->info;
    if (!info) return EOK;
    node->size = info->recv_buf.count;
    return EOK;
}

static errno_t sockfs_free(void *handle) {
    (void)handle;
    return EOK;
}

static int sockfs_mount(const char *handle, vfs_node_t node) {
    if (sockfs_root != NULL) return -EBUSY;
    node->fsid  = sockfs_id;
    sockfs_root = node;
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
    if (sockfs_id == -EINVAL) { kerror("sockfs regist error."); }
    // Auto-create internal root node (not visible in VFS tree)
    sockfs_root = vfs_node_alloc(rootdir, ".sockfs");
    sockfs_root->type   = file_dir;
    sockfs_root->fsid   = sockfs_id;
    sockfs_root->handle = calloc(1, sizeof(socket_specific_t));
}

// Helper: allocate a new socket_info + two VFS nodes + fd
vfs_node_t sockfs_create_node(socket_info_t *info) {
    if (!sockfs_root) return NULL;
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
    spec->shut_rd           = false;
    spec->shut_wr           = false;
    node->handle            = spec;

    return node;
}
