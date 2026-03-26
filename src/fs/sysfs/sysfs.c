#include "fs/sysfs.h"
#include "driver/blk_device.h"
#include "driver/drm/drm.h"
#include "errno.h"
#include "krlibc.h"
#include "net/netlink.h"
#include "term/klog.h"

static int sysfs_id          = 0;
static vfs_node_t sysfs_root = NULL;

// 预建子目录
static vfs_node_t sysfs_class     = NULL;
static vfs_node_t sysfs_devices   = NULL;
static vfs_node_t sysfs_bus       = NULL;
static vfs_node_t sysfs_dev       = NULL;
static vfs_node_t sysfs_block     = NULL;
static vfs_node_t sysfs_kernel    = NULL;
static vfs_node_t sysfs_dev_char  = NULL;
static vfs_node_t sysfs_dev_block = NULL;
static vfs_node_t sysfs_module    = NULL;
static _Atomic(int) next_seq_num  = 1;

static int alloc_seq_num() {
    return next_seq_num++;
}

static errno_t sysfs_mount(const char *src, vfs_node_t node, void *data) {
    sysfs_root       = node;
    sysfs_root->fsid = sysfs_id;

    sysfs_handle_t *root_handle = calloc(1, sizeof(sysfs_handle_t));
    strcpy(root_handle->name, "sysfs");
    root_handle->header.node = node;
    root_handle->header.type = SYSFS_DIR;
    node->handle             = root_handle;

    // 创建标准子目录
    sysfs_class   = sysfs_child_append(node, "class", SYSFS_DIR);
    sysfs_devices = sysfs_child_append(node, "devices", SYSFS_DIR);
    sysfs_bus     = sysfs_child_append(node, "bus", SYSFS_DIR);
    sysfs_dev     = sysfs_child_append(node, "dev", SYSFS_DIR);
    sysfs_block   = sysfs_child_append(node, "block", SYSFS_DIR);
    sysfs_kernel  = sysfs_child_append(node, "kernel", SYSFS_DIR);
    sysfs_module  = sysfs_child_append(node, "module", SYSFS_DIR);

    sysfs_dev_char  = sysfs_child_append(sysfs_dev, "char", SYSFS_DIR);
    sysfs_dev_block = sysfs_child_append(sysfs_dev, "block", SYSFS_DIR);
    sysfs_ensure_dir(sysfs_class, "block");

    sysfs_load_devices_system();
    sysfs_refresh_devices_system();
    sysfs_load_devices_pci();
    sysfs_load_module(sysfs_module);
    drm_sysfs_populate();
    blk_sysfs_populate();

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
    sysfs_header_t *header = (sysfs_header_t *)file;
    if (!header)
        return 0;
    if (header->type == SYSFS_DIR)
        return 0;
    if (header->read)
        return header->read(file, addr, offset, size);
    if (header->type != SYSFS_NONE)
        return 0;

    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (!handle->data)
        return 0;
    if (offset >= handle->size)
        return 0;
    size_t actual = (offset + size > handle->size) ? (handle->size - offset) : size;
    memcpy(addr, handle->data + offset, actual);
    return actual;
}

static size_t sysfs_write(void *file, const void *addr, size_t offset, size_t size) {
    sysfs_header_t *header = (sysfs_header_t *)file;
    if (!header)
        return 0;
    if (header->type == SYSFS_DIR)
        return 0;
    if (header->write)
        return header->write(file, addr, offset, size);
    if (header->type != SYSFS_NONE)
        return 0;

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
    handle->header.node->size = handle->size;
    return size;
}

static errno_t sysfs_stat(void *file, vfs_node_t node) {
    UNUSED(node);
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
    handle->header.node = node;
    handle->header.type = SYSFS_DIR;
    node->handle        = handle;
    node->type |= file_dir;
    return EOK;
}

static errno_t sysfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->header.node = node;
    handle->header.type = SYSFS_NONE;
    node->handle        = handle;
    node->type |= file_none;
    return EOK;
}

static errno_t sysfs_symlink(void *parent, const char *name, vfs_node_t node) {
    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->header.node = node;
    handle->header.type = SYSFS_NONE;
    node->handle        = handle;
    return EOK;
}

static size_t sysfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    if (node == NULL || addr == NULL || size == 0)
        return 0;

    const char *target = node->linkto_path;
    char *resolved     = NULL;
    if (target == NULL && node->linkto != NULL) {
        resolved = vfs_get_fullpath(node->linkto);
        target   = resolved;
        if (target == NULL) {
            free(resolved);
            return 0;
        }
    }
    if (target == NULL) {
        free(resolved);
        return 0;
    }

    size_t len = strlen(target);
    if (offset >= len) {
        free(resolved);
        return 0;
    }

    size_t to_copy = len - offset;
    if (to_copy > size)
        to_copy = size;
    memcpy(addr, target + offset, to_copy);
    free(resolved);
    return to_copy;
}

static errno_t sysfs_free(void *handle) {
    if (!handle)
        return EOK;

    sysfs_header_t *header = handle;
    if (header->type == SYSFS_NONE) {
        sysfs_handle_t *h = handle;
        if (h->data)
            free(h->data);
    }
    free(handle);
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

vfs_node_t sysfs_get_root() {
    return sysfs_root;
}

vfs_node_t sysfs_get_class_root() {
    return sysfs_class;
}

vfs_node_t sysfs_get_devices_root() {
    return sysfs_devices;
}

vfs_node_t sysfs_get_bus_root() {
    return sysfs_bus;
}

vfs_node_t sysfs_get_dev_root() {
    return sysfs_dev;
}

vfs_node_t sysfs_get_block_root() {
    return sysfs_block;
}

vfs_node_t sysfs_get_module_root() {
    return sysfs_module;
}

vfs_node_t sysfs_get_dev_char_root() {
    return sysfs_dev_char;
}

vfs_node_t sysfs_get_dev_block_root() {
    return sysfs_dev_block;
}

static char *sysfs_join_path(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *base = vfs_get_fullpath(parent);
    if (base == NULL) {
        return NULL;
    }

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    size_t path_len = base_len + name_len + 2;
    char *path      = malloc(path_len);
    if (path == NULL) {
        free(base);
        return NULL;
    }

    if (strcmp(base, "/") == 0) {
        sprintf(path, "/%s", name);
    } else {
        sprintf(path, "%s/%s", base, name);
    }

    free(base);
    return path;
}

vfs_node_t sysfs_ensure_dir(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *path = sysfs_join_path(parent, name);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t node = vfs_open(path);
    free(path);
    if (node != NULL) {
        return node;
    }

    return sysfs_child_append(parent, name, SYSFS_DIR);
}

vfs_node_t sysfs_create_file(vfs_node_t parent, const char *name, const char *content) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *path = sysfs_join_path(parent, name);
    vfs_node_t node = path ? vfs_open(path) : NULL;
    free(path);

    if (node == NULL) {
        node = sysfs_child_append(parent, name, SYSFS_NONE);
    }
    if (node == NULL) {
        return NULL;
    }

    if (content != NULL) {
        sysfs_handle_t *handle = node->handle;
        if (handle->data != NULL) {
            free(handle->data);
        }
        size_t len             = strlen(content);
        handle->data           = strdup(content);
        handle->size           = len;
        handle->capacity       = len + 1;
        node->size             = len;
    } else {
        node->size = 0;
    }

    return node;
}

vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, const sysfs_type_t type) {
    if (!parent || !name) {
        return NULL;
    }

    const vfs_node_t node = vfs_node_alloc(parent, name);
    node->fsid            = sysfs_id;
    node->mode            = 0444;
    node->type            = file_none;

    switch (type) {
    case SYSFS_DIR:;
        sysfs_header_t *header = calloc(1, sizeof(sysfs_header_t));
        header->type           = type;
        header->node           = node;
        node->handle           = header;
        node->mode             = 0755;
        node->type             = file_dir;
        return node;
    case SYSFS_MOD:;
        sysfs_module_t *module = calloc(1, sizeof(sysfs_module_t));
        module->header.type    = type;
        module->header.node    = node;
        node->handle           = module;
        node->mode             = 0755;
        node->type             = file_dir;
        return node;
    default:
        break;
    }

    sysfs_handle_t *handle = calloc(1, sizeof(sysfs_handle_t));
    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->header.node = node;
    handle->header.type = SYSFS_NONE;
    node->handle        = handle;
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
    handle->header.node = node;
    handle->header.type = SYSFS_NONE;
    node->handle        = handle;

    return node;
}

vfs_node_t sysfs_child_append_symlink_node(vfs_node_t parent, const char *name, vfs_node_t target) {
    if (target == NULL) {
        return NULL;
    }

    char *path = vfs_get_fullpath(target);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t node = sysfs_child_append_symlink(parent, name, path);
    free(path);
    return node;
}

static void send_netlink_event(
    bool use_dev_root_path,
    const char *dev_root_path,
    const char *real_device_path,
    const char *uevent_content
) {
    const char *event_path = use_dev_root_path ? dev_root_path : real_device_path;
    const char *content    = uevent_content ? uevent_content : "";
    char buffer[256];
    sprintf(
        buffer,
        "add@%s\nACTION=add\nSEQNUM=%d\nTAGS=:systemd:\n%s\n",
        event_path,
        alloc_seq_num(),
        content
    );
    int len = (int)strlen(buffer);
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
        }
    }
    if (!memcmp(buffer + len - 2, "\0\0", 2)) {
        len--;
    }
    netlink_kernel_uevent_send(buffer, len);
}

vfs_node_t sysfs_regist_dev(
    char type,
    int major,
    int minor,
    const char *real_device_path,
    const char *dev_name,
    const char *uevent_content
) {
    UNUSED(dev_name);

    // 在 /sys/dev/char/MAJOR:MINOR 或 /sys/dev/block/MAJOR:MINOR 创建目录
    vfs_node_t dev_parent = (type == 'c') ? sysfs_dev_char : sysfs_dev_block;
    if (!dev_parent) {
        return NULL;
    }

    const char *root = type == 'c' ? "char" : "block";
    char dev_root_path[256];
    sprintf(dev_root_path, "/sys/dev/%s/%d:%d", root, major, minor);
    bool has_real_device_path = real_device_path != NULL && real_device_path[0] != '\0';

    char dev_nr_name[32];
    sprintf(dev_nr_name, "%d:%d", major, minor);

    vfs_node_t dev_root = has_real_device_path
                              ? sysfs_child_append_symlink(dev_parent, dev_nr_name, real_device_path)
                              : sysfs_child_append(dev_parent, dev_nr_name, SYSFS_DIR);
    if (dev_root == NULL) {
        return NULL;
    }

    if (!has_real_device_path && uevent_content && uevent_content[0] != '\0') {
        sysfs_create_file(dev_root, "uevent", uevent_content);
    }

    send_netlink_event(!has_real_device_path, dev_root_path, real_device_path, uevent_content);
    return dev_root;
}
