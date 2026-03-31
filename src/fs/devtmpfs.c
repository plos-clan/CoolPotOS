#include "fs/devtmpfs.h"
#include "boot.h"
#include "cow_arraylist.h"
#include "driver/blk_device.h"
#include "driver/drm/drm_device.h"
#include "driver/evdev.h"
#include "driver/fb.h"
#include "driver/tty.h"
#include "errno.h"
#include "lib/sprintf.h"
#include "string_builder.h"
#include "task/poll.h"
#include "term/klog.h"

int dev_tmpfs_id                          = 0;
static _Atomic volatile size_t dev_id_now = 0;

static void load_tty_device(vfs_node_t node) {
    int tty_id = 1;

    tty_t *kernel_session = get_kernel_session();
    create_device_node(
        node,
        "tty0",
        device_stream,
        kernel_session,
        0,
        (void *)kernel_session->ops.ioctl,
        (void *)kernel_session->ops.read,
        (void *)kernel_session->ops.write,
        (void *)kernel_session->ops.poll,
        NULL,
        (void *)kernel_session->ops.size_t
    );

    tty_t *pos = NULL;
    tty_t *n   = NULL;
    llist_for_each(pos, n, get_tty_session_list(), list_node) {
        if (pos == kernel_session) {
            continue;
        }
        char name[10];
        if (pos->device->type == TTY_DEVICE_SERIAL) {
            sprintf(name, "ttyS%d", tty_id++);
        } else {
            sprintf(name, "tty%d", tty_id++);
        }

        create_device_node(
            node,
            name,
            device_stream,
            pos,
            0,
            (void *)pos->ops.ioctl,
            (void *)pos->ops.read,
            (void *)pos->ops.write,
            (void *)pos->ops.poll,
            NULL,
            (void *)pos->ops.size_t
        );
    }
}

static void load_blk_device(vfs_node_t node) {
    blk_device_t *device = NULL;
    cow_foreach(get_block_device_list(), device) {
        create_device_node(
            node,
            device->name,
            blk_device_is_stream(device) ? device_stream : device_block,
            device,
            blk_device_dev_number(device),
            (void *)blk_ioctl,
            (void *)blk_device_read,
            (void *)blk_device_write,
            (void *)blk_poll,
            NULL,
            (void *)blk_size_t
        );
    }
}

static void load_drm_device(vfs_node_t node) {
    char *full_path           = vfs_get_fullpath(node);
    string_builder_t *builder = create_string_builder(50);
    string_builder_append(builder, "%s/dri", full_path);
    vfs_mkdir(builder->data);
    const vfs_node_t drm_dir = vfs_open(builder->data);

    const drmd_device_t *device = NULL;
    cow_foreach(drm_devices_get(), device) {
        create_device_node(
            drm_dir,
            device->name,
            device_stream,
            device->ptr,
            device->dev,
            device->ioctl,
            device->read,
            device->write,
            device->poll,
            device->map,
            drm_size_t
        );
    }

    vfs_close(drm_dir);
    free(full_path);
    free(builder->data);
    free(builder);
}

errno_t devtmpfs_mount(const char *handle, vfs_node_t node, void *data) {
    node->fsid                = dev_tmpfs_id;
    dtmp_handle_t *tmpfs_root = malloc(sizeof(dtmp_handle_t));
    tmpfs_root->type          = dtp_file_dir;
    tmpfs_root->node          = node;
    tmpfs_root->root          = node;
    strcpy(tmpfs_root->name, "tmp");
    node->handle = tmpfs_root;

    load_tty_device(node);
    load_blk_device(node);
    load_drm_device(node);
    fb_setup(node);
    evdev_setup(node);

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
    if (handle == NULL)
        return EOK;
    dtmp_handle_t *file = handle;
    if (file->type == dtp_file_device && file->is_per_open) {
        if (file->close_t != NULL && file->device_handle != NULL) {
            file->close_t(file->device_handle);
            file->device_handle = NULL;
        }
    }
    if (file->type != dtp_file_file) {
        free(file);
        return EOK;
    }
    if (file->data != NULL)
        free(file->data);
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

void devtmpfs_open(void *parent, const char *name, vfs_node_t node) {
    dtmp_handle_t *f = (dtmp_handle_t *)node->handle;
    if (f && f->type == dtp_file_device && f->open_t) {
        f->open_t(parent, name, node);
    }
}

errno_t devtmpfs_rename(void *current, const char *new_name) {
    dtmp_handle_t *f = (dtmp_handle_t *)current;
    if (f->type == dtp_file_device)
        return -EPERM;
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

    // For devices with open_t callback, call it to initialize per-open instance
    dtmp_handle_t *dtmp = (dtmp_handle_t *)node->handle;
    if (dtmp && dtmp->type == dtp_file_device && dtmp->open_t) {
        dtmp->open_t(node->parent ? node->parent->handle : NULL, node->name, copy);
    }

    return copy;
}

size_t devtmpfs_read(void *file, void *addr, size_t offset, size_t size) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device)
        return f->read_t(f->device_handle, addr, offset, size);
    if (offset >= f->size)
        return 0;
    size_t actual = (offset + size > f->size) ? (f->size - offset) : size;
    memcpy(addr, f->data + offset, actual);
    return actual;
}

size_t devtmpfs_write(void *file, const void *addr, size_t offset, size_t size) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device)
        return f->write_t(f->device_handle, addr, offset, size);
    size_t end = offset + size;
    if (end > f->capacity) {
        size_t new_cap = end * 2;
        char *new_buf  = realloc(f->data, new_cap);
        if (!new_buf)
            return 0;
        f->data     = new_buf;
        f->capacity = new_cap;
    }
    memcpy(f->data + offset, addr, size);
    if (end > f->size)
        f->size = end;
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

bool devtmpfs_close(void *file) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f && f->type == dtp_file_device && f->close_t && f->device_handle) {
        f->close_t(f->device_handle);
    }
    return false;
}

int devtmpfs_poll(void *file, size_t events) {
    dtmp_handle_t *f = (dtmp_handle_t *)file;
    if (f->type == dtp_file_device)
        return f->poll_t(f->device_handle, events);
    int revents = 0;
    if (events & POLLIN)
        revents |= POLLIN;
    if (events & POLLOUT)
        revents |= POLLOUT;
    return revents;
}

errno_t devtmpfs_stat(void *file, vfs_node_t node) {
    dtmp_handle_t *file0 = (dtmp_handle_t *)file;
    if (file0 == NULL)
        return -ENOENT;
    if (file0->type == dtp_file_device) {
        // Only set type if open_t didn't set a specific type (e.g. file_ptmx)
        if (!(node->type & (file_ptmx | file_pts))) {
            node->type = file0->dev_type == device_stream ? file_stream : file_block;
        }
        if (file0->size_t)
            node->size = file0->size_t(file0->device_handle);
        if (file0->dev_type == device_block && file0->device_handle != NULL) {
            blk_device_t *device = file0->device_handle;
            node->blksz          = device->block_size;
        }
        return EOK;
    }
    node->type = file0->type == dtp_file_symlink ? file_symlink
                 : file0->type == dtp_file_dir   ? file_dir
                                                 : file_none;
    node->size = file0->type == dtp_file_dir ? 0 : file0->size;
    return EOK;
}

errno_t create_device_node(
    vfs_node_t root,
    char *name,
    enum device_type type,
    void *handle,
    uint64_t dev_number,
    vfs_ioctl_t ioctl,
    vfs_read_t read,
    vfs_write_t write,
    vfs_poll_t poll,
    vfs_mapfile_t map,
    size_t (*size_t)(void *handle)
) {
    return create_device_node_ex(
        root, name, type, handle, dev_number, NULL, NULL, ioctl, read, write, poll, map, size_t
    );
}

errno_t create_device_node_ex(
    vfs_node_t root,
    char *name,
    enum device_type type,
    void *handle,
    uint64_t dev_number,
    void (*open_t)(void *, const char *, vfs_node_t),
    vfs_close_t close_t,
    vfs_ioctl_t ioctl,
    vfs_read_t read,
    vfs_write_t write,
    vfs_poll_t poll,
    vfs_mapfile_t map,
    size_t (*size_t)(void *handle)
) {
    if (root == NULL)
        return -EINVAL;
    if (root->fsid != dev_tmpfs_id)
        return -ENODEV;
    char *full_path  = vfs_get_fullpath(root);
    char *creat_path = calloc(1, strlen(full_path) + strlen(name) + 5);
    sprintf(creat_path, "%s/%s", full_path, name);
    if (vfs_mkfile(creat_path) != EOK)
        goto err;
    vfs_node_t node = vfs_open(creat_path);
    if (node == NULL)
        goto err;
    dtmp_handle_t *fs_handle = node->handle;
    not_null_assert(fs_handle, "devtmpfs: create device handle null.");
    fs_handle->dev_type    = type;
    fs_handle->type        = dtp_file_device;
    fs_handle->open_t      = open_t;
    fs_handle->close_t     = close_t;
    fs_handle->is_per_open = false;
    fs_handle->ioctl_t     = ioctl;
    fs_handle->read_t      = read;
    fs_handle->write_t     = write;
    fs_handle->mapfile_t   = map;
    fs_handle->poll_t      = poll;
    // Only set device_handle if open_t hasn't already set it
    // For devices with open_t (like ptmx), open_t will set device_handle per-open
    // For devices without open_t, set it to the provided handle or self-reference
    if (!open_t) {
        fs_handle->device_handle = handle == NULL ? fs_handle : handle;
    } else {
        // Leave NULL for now; open_t will set it during stat
        fs_handle->device_handle = NULL;
    }
    fs_handle->size_t = size_t;
    node->size        = size_t ? size_t(open_t ? NULL : handle) : 0;
    node->dev         = dev_number ? dev_number : dev_id_now++;
    node->rdev        = node->dev;
    node->type        = fs_handle->dev_type == device_stream ? file_stream : file_block;
    vfs_close(node);
    free(creat_path);
    free(full_path);
    return EOK;
err:;
    kerror("Cannot create device %s", creat_path);
    free(full_path);
    free(creat_path);
    return -EIO;
}

errno_t devtmpfs_chmod(vfs_node_t node, uint16_t mode) {
    node->mode = mode;
    return EOK;
}

errno_t devtmpfs_mknod(void *parent, const char *name, vfs_node_t node, uint16_t mode, int dev) {
    node->dev             = 0;
    node->rdev            = dev;
    node->mode            = mode & 0777;
    dtmp_handle_t *handle = malloc(sizeof(dtmp_handle_t));
    handle->size          = 0;
    handle->node          = node;
    if ((mode & S_IFMT) == S_IFBLK) {
        node->type   = file_block;
        handle->type = dtp_file_device;
    }
    if ((mode & S_IFMT) == S_IFCHR) {
        node->type   = file_stream;
        handle->type = dtp_file_device;
    } else {
        node->type   = file_none;
        handle->type = dtp_file_file;
    }
    strncpy(handle->name, name, 64);
    node->handle = handle;
    return 0;
}

size_t devtmpfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    if (node == NULL || addr == NULL || size == 0)
        return 0;
    const char *target = node->linkto_path;
    if (target == NULL && node->linkto != NULL) {
        target = vfs_get_fullpath(node->linkto);
        if (target == NULL)
            return 0;
    }
    if (target == NULL)
        return 0;
    size_t len = strlen(target);
    if (offset >= len) {
        if (target != node->linkto_path)
            free((void *)target);
        return 0;
    }
    size_t to_copy = len - offset;
    if (to_copy > size)
        to_copy = size;
    memcpy(addr, target + offset, to_copy);
    if (target != node->linkto_path)
        free((void *)target);
    return to_copy;
}

errno_t devtmpfs_ioctl(void *file, size_t req, void *arg) {
    dtmp_handle_t *handle = file;
    if (handle->ioctl_t == NULL)
        return -ENOSYS;
    return handle->ioctl_t(handle->device_handle, req, arg);
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
    .readlink = devtmpfs_readlink,
    .mkfile   = devtmpfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = devtmpfs_symlink,
    .ioctl    = devtmpfs_ioctl,
    .dup      = devtmpfs_dup,
    .delete   = devtmpfs_delete,
    .rename   = devtmpfs_rename,
    .poll     = devtmpfs_poll,
    .map      = devtmpfs_map,
    .free     = devtmpfs_free,
    .chmod    = devtmpfs_chmod,
    .mknod    = devtmpfs_mknod,
};

void devtmpfs_regist() {
    dev_tmpfs_id = vfs_regist("devtmpfs", &devtmpfs_callbacks, 0x01021994, FS_VIRTUAL_FLAGS);
    if (dev_tmpfs_id & ERRNO_MASK) {
        kerror("devtmpfs register error");
    }
}
