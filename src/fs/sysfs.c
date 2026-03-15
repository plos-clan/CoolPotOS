#include "fs/sysfs.h"
#include "driver/drm/drm.h"
#include "errno.h"
#include "krlibc.h"
#include "term/klog.h"

static int sysfs_id          = 0;
static vfs_node_t sysfs_root = NULL;

// 预建子目录
static vfs_node_t sysfs_class     = NULL;
static vfs_node_t sysfs_devices   = NULL;
static vfs_node_t sysfs_bus       = NULL;
static vfs_node_t sysfs_dev       = NULL;
static vfs_node_t sysfs_kernel    = NULL;
static vfs_node_t sysfs_dev_char  = NULL;
static vfs_node_t sysfs_dev_block = NULL;

static errno_t sysfs_mount(const char *src, vfs_node_t node, void *data) {
    sysfs_root       = node;
    sysfs_root->fsid = sysfs_id;

    sysfs_handle_t *root_handle = calloc(1, sizeof(sysfs_handle_t));
    strcpy(root_handle->name, "sysfs");
    root_handle->node = node;
    node->handle      = root_handle;

    // 创建标准子目录
    sysfs_class   = sysfs_child_append(node, "class", true);
    sysfs_devices = sysfs_child_append(node, "devices", true);
    sysfs_bus     = sysfs_child_append(node, "bus", true);
    sysfs_dev     = sysfs_child_append(node, "dev", true);
    sysfs_kernel  = sysfs_child_append(node, "kernel", true);

    sysfs_dev_char  = sysfs_child_append(sysfs_dev, "char", true);
    sysfs_dev_block = sysfs_child_append(sysfs_dev, "block", true);

    // /sys/bus/pci
    sysfs_child_append(sysfs_bus, "pci", true);
    drm_sysfs_populate();

    return EOK;
}

static void sysfs_open(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent, name, node);
}

static bool sysfs_close(void *current) {
    UNUSED(current);
    return false;
}

static size_t sysfs_read(void *file, void *addr, size_t offset, size_t size) {
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (!handle || !handle->data)
        return 0;
    if (offset >= handle->size)
        return 0;
    size_t actual = (offset + size > handle->size) ? (handle->size - offset) : size;
    memcpy(addr, handle->data + offset, actual);
    return actual;
}

static size_t sysfs_write(void *file, const void *addr, size_t offset, size_t size) {
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (!handle)
        return 0;
    size_t end = offset + size;
    if (end > handle->capacity) {
        size_t new_cap = end + 256;
        char *new_buf  = realloc(handle->data, new_cap);
        if (!new_buf)
            return 0;
        handle->data     = new_buf;
        handle->capacity = new_cap;
    }
    memcpy(handle->data + offset, addr, size);
    if (end > handle->size)
        handle->size = end;
    handle->node->size = handle->size;
    return size;
}

static errno_t sysfs_stat(void *file, vfs_node_t node) {
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (!handle)
        return -ENOENT;
    return EOK;
}

static vfs_node_t sysfs_dup(vfs_node_t node) {
    return node;
}

static errno_t sysfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->node = node;
    node->handle = handle;
    node->type |= file_dir;
    return EOK;
}

static errno_t sysfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->node = node;
    node->handle = handle;
    node->type |= file_none;
    return EOK;
}

static errno_t sysfs_symlink(void *parent, const char *name, vfs_node_t node) {
    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->node = node;
    node->handle = handle;
    return EOK;
}

static size_t sysfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
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
    if (offset >= len)
        return 0;
    size_t to_copy = len - offset;
    if (to_copy > size)
        to_copy = size;
    memcpy(addr, target + offset, to_copy);
    return to_copy;
}

static errno_t sysfs_free(void *handle) {
    if (!handle)
        return EOK;
    sysfs_handle_t *h = handle;
    if (h->data)
        free(h->data);
    free(h);
    return EOK;
}

static struct vfs_callback sysfs_callbacks = {
    .mount    = sysfs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = sysfs_open,
    .close    = sysfs_close,
    .read     = sysfs_read,
    .write    = sysfs_write,
    .readlink = sysfs_readlink,
    .mkdir    = sysfs_mkdir,
    .mkfile   = sysfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = sysfs_symlink,
    .stat     = sysfs_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = sysfs_dup,
    .poll     = (vfs_poll_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .free     = (vfs_free_t)sysfs_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void sysfs_regist() {
    sysfs_id = vfs_regist("sysfs", &sysfs_callbacks, 0x62656572, FS_VIRTUAL_FLAGS);
    if (sysfs_id & ERRNO_MASK) {
        kerror("sysfs register error");
    }
}

vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, bool is_dir) {
    if (!parent || !name)
        return NULL;

    vfs_node_t node = vfs_node_alloc(parent, name);
    node->fsid      = sysfs_id;
    node->mode      = is_dir ? 0755 : 0444;

    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->node = node;
    node->handle = handle;
    node->type   = is_dir ? file_dir : file_none;

    return node;
}

vfs_node_t sysfs_child_append_symlink(vfs_node_t parent, const char *name, const char *target) {
    if (!parent || !name || !target)
        return NULL;

    vfs_node_t node   = vfs_node_alloc(parent, name);
    node->fsid        = sysfs_id;
    node->mode        = 0777;
    node->type        = file_symlink;
    node->linkto_path = strdup(target);

    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->node = node;
    node->handle = handle;

    return node;
}

vfs_node_t sysfs_regist_dev(
    char type,
    int major,
    int minor,
    const char *bus_path,
    const char *dev_name,
    const char *uevent_content
) {
    // 在 /sys/dev/char/MAJOR:MINOR 或 /sys/dev/block/MAJOR:MINOR 创建目录
    vfs_node_t dev_parent = (type == 'c') ? sysfs_dev_char : sysfs_dev_block;
    if (!dev_parent)
        return NULL;

    char dev_nr_name[32];
    sprintf(dev_nr_name, "%d:%d", major, minor);

    vfs_node_t dev_root = sysfs_child_append(dev_parent, dev_nr_name, true);

    // 创建 uevent 文件
    if (uevent_content && strlen(uevent_content) > 0) {
        vfs_node_t uevent = sysfs_child_append(dev_root, "uevent", false);
        sysfs_handle_t *h = uevent->handle;
        size_t len        = strlen(uevent_content);
        h->data           = strdup(uevent_content);
        h->size           = len;
        h->capacity       = len + 1;
        uevent->size      = len;
    }

    return dev_root;
}
