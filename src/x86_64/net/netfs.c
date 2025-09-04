#include "llist.h"
#include "network.h"
#include "vfs.h"

struct llist_header net_root = {0};

errno_t netfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (void *)NETFS_REGISTER_ID) return VFS_STATUS_FAILED;
    return VFS_STATUS_SUCCESS;
}

void netfs_open(void *handle, const char *name, vfs_node_t node) {}

struct vfs_callback netfs_callbacks = {
    .mount    = netfs_mount,
    .unmount  = NULL,
    .open     = netfs_open,
    .close    = NULL,
    .read     = NULL,
    .write    = NULL,
    .readlink = NULL,
    .mkdir    = NULL,
    .mkfile   = NULL,
    .delete   = NULL,
    .rename   = NULL,
    .stat     = NULL,
    .ioctl    = NULL,
    .dup      = NULL,
    .poll     = NULL,
    .map      = NULL,
};

void netfs_setup() {}
