#include "../../../include/llist.h"
#include "../../../module/all_include/cp_kernel.h"
#include "errno.h"
#include "network.h"
#include "pcb.h"
#include "vfs.h"

struct llist_header net_root         = {0};
int                 unix_socket_fsid = 0;
int                 unix_accept_fsid = 0;
vfs_node_t          sockfs_root      = NULL;

static errno_t dummy() {
    return EOK;
}

static errno_t udummy() {
    return -1;
}

errno_t netfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (void *)NETFS_REGISTER_ID) return VFS_STATUS_FAILED;
    node->fsid  = unix_socket_fsid;
    sockfs_root = node;
    return VFS_STATUS_SUCCESS;
}

void socket_open(void *handle, const char *name, vfs_node_t node) {}

errno_t socket_stat(void *file, vfs_node_t node) {
    return EOK;
}

vfs_node_t socket_dup(vfs_node_t node) {
    socket_handle_t *handle = node->handle;
    socket_t        *socket = handle->sock;
    // socket->timesOpened++;
    // if (socket->pair)
    // {
    //     socket->pair->clientFds++;
    // }
    return node;
}

size_t socket_read(void *f, void *buf, size_t offset, size_t limit) {
    UNUSED(offset);

    socket_handle_t    *handle = f;
    socket_t           *sock   = handle->sock;
    unix_socket_pair_t *pair   = sock->pair;
    while (true) {
        if (!pair->clientFds && pair->clientBuffPos == 0) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -EPIPE;
        } else if ((handle->fd->flags & O_NONBLOCK) && pair->clientBuffPos == 0) {
            return -(EWOULDBLOCK);
        } else if (pair->clientBuffPos > 0)
            break;

        scheduler_yield();
    }

    spin_lock(pair->lock);

    size_t toCopy = MIN(limit, pair->clientBuffPos);
    memcpy(buf, pair->clientBuff, toCopy);
    memmove(pair->clientBuff, &pair->clientBuff[toCopy], pair->clientBuffPos - toCopy);
    pair->clientBuffPos -= toCopy;

    spin_unlock(pair->lock);

    return toCopy;
}

size_t socket_write(void *f, const void *buf, size_t offset, size_t limit) {
    UNUSED(offset);

    socket_handle_t    *handle = f;
    socket_t           *sock   = handle->sock;
    unix_socket_pair_t *pair   = sock->pair;

    if (limit > pair->serverBuffSize) { limit = pair->serverBuffSize; }

    while (true) {
        if (!pair->serverFds) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -(EPIPE);
        } else if ((handle->fd->flags & O_NONBLOCK) &&
                   (pair->serverBuffPos + limit) > pair->serverBuffSize) {
            return -(EWOULDBLOCK);
        }

        if ((pair->serverBuffPos + limit) <= pair->serverBuffSize) break;

        scheduler_yield();
    }

    close_interrupt;

    spin_lock(pair->lock);

    limit = MIN(limit, pair->serverBuffSize);
    memcpy(&pair->serverBuff[pair->serverBuffPos], buf, limit);
    pair->serverBuffPos += limit;

    spin_unlock(pair->lock);

    return limit;
}

size_t socket_accept_read(void *f, void *buf, size_t offset, size_t limit) {
    UNUSED(offset);

    socket_handle_t    *handle = f;
    unix_socket_pair_t *pair   = handle->sock;
    while (true) {
        if (!pair->clientFds && pair->serverBuffPos == 0) {
            spin_lock(get_current_task()->signal_lock);
            get_current_task()->signal |= SIGMASK(SIGPIPE);
            spin_unlock(get_current_task()->signal_lock);
            return -EPIPE;
        } else if ((handle->fd->flags & O_NONBLOCK) && pair->serverBuffPos == 0) {
            return -(EWOULDBLOCK);
        } else if (pair->serverBuffPos > 0)
            break;

        scheduler_yield();
    }

    spin_lock(pair->lock);

    size_t toCopy = MIN(limit, pair->serverBuffPos);
    memcpy(buf, pair->serverBuff, toCopy);
    memmove(pair->serverBuff, &pair->serverBuff[toCopy], pair->serverBuffPos - toCopy);
    pair->serverBuffPos -= toCopy;

    spin_unlock(pair->lock);

    return toCopy;
}

size_t socket_accept_write(void *f, const void *buf, size_t offset, size_t limit) {
    UNUSED(offset);

    socket_handle_t    *handle = f;
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

        if (handle->fd->flags & O_NONBLOCK) { return -(EWOULDBLOCK); }

        scheduler_yield();
    }

    close_interrupt;

    spin_lock(pair->lock);

    limit = MIN(limit, pair->clientBuffSize);
    memcpy(&pair->clientBuff[pair->clientBuffPos], buf, limit);
    pair->clientBuffPos += limit;

    spin_unlock(pair->lock);

    return limit;
}

vfs_node_t socket_accept_dup(vfs_node_t node) {
    socket_handle_t    *handle = node->handle;
    unix_socket_pair_t *pair   = handle->sock;
    // pair->serverFds++;
    return node;
}

struct vfs_callback netfs_callbacks = {
    .mount    = netfs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = socket_open,
    .close    = socket_socket_close,
    .read     = socket_read,
    .write    = socket_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .stat     = socket_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = socket_dup,
    .poll     = socket_socket_poll,
    .map      = (vfs_mapfile_t)dummy,
};

static struct vfs_callback accept_callback = {
    .mount    = (vfs_mount_t)udummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = socket_open,
    .close    = socket_accept_close,
    .read     = socket_accept_read,
    .write    = socket_accept_write,
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
    .poll     = socket_accept_poll,
    .dup      = socket_accept_dup,
};

void netfs_setup() {
    unix_socket_fsid = vfs_regist("sockfs", &netfs_callbacks, NETFS_REGISTER_ID, 0x534F434B);
    unix_accept_fsid = vfs_regist("acceptfs", &accept_callback, NETFS_REGISTER_ID, 0x534F434B);
}
