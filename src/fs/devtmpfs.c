#include "fs/devtmpfs.h"
#include "driver/tty.h"
#include "errno.h"
#include "lib/sprintf.h"
#include "task/poll.h"
#include "term/klog.h"

int dev_tmpfs_id = 0;

errno_t devtmpfs_mount(const char *handle, vfs_node_t node) {
    node->fsid                = dev_tmpfs_id;
    dtmp_handle_t *tmpfs_root = (dtmp_handle_t *)malloc(sizeof(dtmp_handle_t));
    tmpfs_root->type          = dtp_file_dir;
    tmpfs_root->node          = node;
    tmpfs_root->root          = node;
    strcpy(tmpfs_root->name, "tmp");
    node->handle = tmpfs_root;

    extern tty_t *kernel_session;
    create_device_node(node, "stdout", device_stream, kernel_session,
                       (void *)kernel_session->ops.ioctl, (void *)kernel_session->ops.read,
                       (void *)kernel_session->ops.write, (void *)kernel_session->ops.poll, NULL,
                       (void *)kernel_session->ops.size_t);
    create_device_node(node, "stderr", device_stream, kernel_session,
                       (void *)kernel_session->ops.ioctl, (void *)kernel_session->ops.read,
                       (void *)kernel_session->ops.write, (void *)kernel_session->ops.poll, NULL,
                       (void *)kernel_session->ops.size_t);
    create_device_node(node, "stdin", device_stream, kernel_session,
                       (void *)kernel_session->ops.ioctl, (void *)kernel_session->ops.read,
                       (void *)kernel_session->ops.write, (void *)kernel_session->ops.poll, NULL,
                       (void *)kernel_session->ops.size_t);
    return EOK;
}

void devtmpfs_umount(void *root) {
    dtmp_handle_t *tmpfs_root = root;
    vfs_free(tmpfs_root->node);
}

errno_t devtmpfs_symlink(void *parent, const char *name, vfs_node_t node) {
    dtmp_handle_t *f = calloc(1, sizeof(dtmp_handle_t));
    strncpy(f->name, name, sizeof(f->name));
    f->type      = dtp_file_symlink;
    node->handle = f;
    f->node      = node;
    return EOK;
}

errno_t devtmpfs_free(void *handle) {
    dtmp_handle_t *file = handle;
    if (file->type != dtp_file_file) {
        free(file);
        return EOK;
    }
    if (file->data != NULL) free(file->data);
    free(file);
    return EOK;
}

errno_t devtmpfs_mk(void *parent, const char *name, vfs_node_t node, bool is_dir) {
    dtmp_handle_t *f = calloc(1, sizeof(dtmp_handle_t));
    strncpy(f->name, name, sizeof(f->name));
    f->type      = is_dir ? dtp_file_dir : dtp_file_file;
    node->handle = f;
    f->node      = node;
    return EOK;
}

errno_t devtmpfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    return devtmpfs_mk(parent, name, node, true);
}

errno_t devtmpfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    return devtmpfs_mk(parent, name, node, false);
}

errno_t devtmpfs_delete(void *parent, vfs_node_t node) {
    dtmp_handle_t *f = (dtmp_handle_t *)node->handle;
    free(f->data);
    free(f);
    return EOK;
}

void devtmpfs_open(void *parent, const char *name, vfs_node_t node) {}

errno_t devtmpfs_rename(void *current, const char *new_name) {
    dtmp_handle_t *f = (dtmp_handle_t *)current;
    if (f->type == dtp_file_device) return -EPERM;
    strncpy(f->name, new_name, sizeof(f->name));
    return EOK;
}

vfs_node_t devtmpfs_dup(vfs_node_t node) {
    vfs_node_t copy   = vfs_node_alloc(node->parent, node->name);
    copy->handle      = node->handle;
    copy->type        = node->type;
    copy->size        = node->size;
    copy->linkname    = node->linkname == NULL ? NULL : strdup(node->linkname);
    copy->flags       = node->flags;
    copy->permissions = node->permissions;
    copy->owner       = node->owner;
    copy->child       = node->child;
    copy->realsize    = node->realsize;
    return copy;
}

size_t devtmpfs_read(void *file, void *addr, size_t offset, size_t size) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device) return f->read_t(f->device_handle, addr, offset, size);
    if (offset >= f->size) return 0;
    size_t actual = (offset + size > f->size) ? (f->size - offset) : size;
    memcpy(addr, f->data + offset, actual);
    return actual;
}

size_t devtmpfs_write(void *file, const void *addr, size_t offset, size_t size) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device) return f->write_t(f->device_handle, addr, offset, size);
    size_t end = offset + size;
    if (end > f->capacity) {
        size_t new_cap = end * 2;
        char  *new_buf = realloc(f->data, new_cap);
        if (!new_buf) return 0;
        f->data     = new_buf;
        f->capacity = new_cap;
    }
    memcpy(f->data + offset, addr, size);
    if (end > f->size) f->size = end;
    f->node->size = f->size;
    return size;
}

void *devtmpfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    dtmp_handle_t *handle = file;
    if (handle->type == dtp_file_device) {
        return handle->mapfile_t(handle->device_handle, addr, offset, size, prot, flags);
    } else
        return general_map(devtmpfs_read, file, (uint64_t)addr, size, prot, flags, offset);
}

void devtmpfs_close(void *file) {}

int devtmpfs_poll(void *file, size_t events) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device) return f->poll_t(f->device_handle, events);
    int revents = 0;
    if (events & POLLIN) revents |= POLLIN;
    if (events & POLLOUT) revents |= POLLOUT;
    return revents;
}

errno_t devtmpfs_stat(void *file, vfs_node_t node) {
    dtmp_handle_t *file0 = (dtmp_handle_t *)file;
    if (file0 == NULL) return -ENOENT;
    if (file0->type == dtp_file_device) {
        node->type = file0->dev_type == device_stream ? file_stream : file_block;
        return EOK;
    }
    node->type = file0->type == dtp_file_symlink ? file_symlink
                 : file0->type == dtp_file_dir   ? file_dir
                                                 : file_none;
    node->size = file0->type == dtp_file_dir ? 0 : file0->size_t(file0->device_handle);
    return EOK;
}

errno_t create_device_node(vfs_node_t root, char *name, enum device_type type, void *handle,
                           vfs_ioctl_t ioctl, vfs_read_t read, vfs_write_t write, vfs_poll_t poll,
                           vfs_mapfile_t map, size_t (*size_t)(void *handle)) {
    if (root == NULL) return -EINVAL;
    if (root->fsid != dev_tmpfs_id) return -ENODEV;
    char *full_path  = vfs_get_fullpath(root);
    char *creat_path = calloc(1, strlen(full_path) + strlen(name) + 1);
    sprintf(creat_path, "%s/%s", full_path, name);
    if (vfs_mkdir(creat_path) != EOK) goto err;
    vfs_node_t node = vfs_open(creat_path);
    if (node == NULL) goto err;
    dtmp_handle_t *fs_handle = node->handle;
    not_null_assert(fs_handle, "devtmpfs: create device handle null.");
    fs_handle->dev_type      = type;
    fs_handle->type          = dtp_file_device;
    fs_handle->ioctl_t       = ioctl;
    fs_handle->read_t        = read;
    fs_handle->write_t       = write;
    fs_handle->mapfile_t     = map;
    fs_handle->poll_t        = poll;
    fs_handle->device_handle = handle;
    fs_handle->size_t        = size_t;
    logkf("devtmpfs: create device at %s\n\r", creat_path);
    node->size = size_t(handle);
    vfs_update(node);
    vfs_close(node);
    return EOK;
err:;
    kerror("Cannot create device %s", creat_path);
    free(full_path);
    free(creat_path);
    return -EIO;
}

static struct vfs_callback devtmpfs_callbacks = {
    .mount    = devtmpfs_mount,
    .unmount  = devtmpfs_umount,
    .mkdir    = devtmpfs_mkdir,
    .close    = devtmpfs_close,
    .stat     = devtmpfs_stat,
    .open     = devtmpfs_open,
    .read     = devtmpfs_read,
    .write    = devtmpfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = devtmpfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = devtmpfs_symlink,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = devtmpfs_dup,
    .delete   = devtmpfs_delete,
    .rename   = devtmpfs_rename,
    .poll     = devtmpfs_poll,
    .map      = devtmpfs_map,
    .free     = devtmpfs_free,
};

void devtmpfs_regist() {
    dev_tmpfs_id = vfs_regist("devtmpfs", &devtmpfs_callbacks, 0x01021994);
    if (dev_tmpfs_id & ERRNO_MASK) { kerror("devtmpfs register error"); }
}
