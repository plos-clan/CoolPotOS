#include "fs/tmpfs.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/page.h"
#include "task/poll.h"
#include "term/klog.h"

static int tmpfs_id              = 0;
static _Atomic int mount_dev_now = 0;

errno_t tmpfs_mount(const char *handle, vfs_node_t node, void *data) {
    node->fsid               = tmpfs_id;
    tmpfs_file_t *tmpfs_root = malloc(sizeof(tmpfs_file_t));
    tmpfs_root->type         = tp_file_dir;
    tmpfs_root->link_count   = 1;
    tmpfs_root->node         = node;
    tmpfs_root->root         = node;
    strcpy(tmpfs_root->name, "tmp");
    node->handle = tmpfs_root;
    node->dev    = mount_dev_now++;
    return EOK;
}

void tmpfs_umount(void *root) {
    tmpfs_file_t *tmpfs_root = root;
    vfs_free(tmpfs_root->node);
}

errno_t tmpfs_mk(void *parent, const char *name, vfs_node_t node, bool is_dir) {
    tmpfs_file_t *f = calloc(1, sizeof(tmpfs_file_t));
    strncpy(f->name, name, sizeof(f->name));
    f->type       = is_dir ? tp_file_dir : tp_file_file;
    f->link_count = 1;
    node->type |= is_dir ? file_dir : file_none;
    node->handle = f;
    f->node      = node;
    return EOK;
}

errno_t tmpfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    return tmpfs_mk(parent, name, node, true);
}

errno_t tmpfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    return tmpfs_mk(parent, name, node, false);
}

errno_t tmpfs_link(void *parent, const char *name, vfs_node_t node) {
    if (name == NULL || node == NULL)
        return -EINVAL;
    char *target_path = name[0] == '/' ? strdup(name) : vfs_cwd_path_build((char *)name);
    if (target_path == NULL)
        return -ENOENT;
    vfs_node_t target = vfs_open(target_path);
    free(target_path);
    if (target == NULL || target->handle == NULL || (target->type & file_dir))
        return -ENOENT;

    tmpfs_file_t *target_file = target->handle;
    target_file->link_count++;

    node->handle = target_file;
    node->type   = target->type;
    node->size   = target->size;
    node->inode  = target->inode;
    node->mode   = target->mode;
    node->owner  = target->owner;
    node->group  = target->group;
    return EOK;
}

size_t tmpfs_read(void *file, void *addr, size_t offset, size_t size) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;

    if (offset >= f->size)
        return 0;
    size_t actual = (offset + size > f->size) ? (f->size - offset) : size;
    memcpy(addr, f->data + offset, actual);
    return actual;
}

size_t tmpfs_write(void *file, const void *addr, size_t offset, size_t size) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;
    size_t end      = offset + size;
    if (end > f->capacity) {
        size_t new_cap = end + PAGE_SIZE;
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

errno_t tmpfs_stat(void *file, vfs_node_t node) {
    tmpfs_file_t *file0 = (tmpfs_file_t *)file;
    if (file0 == NULL)
        return -ENOENT;
    node->type = file0->type == tp_file_symlink ? file_symlink
                 : file0->type == tp_file_dir   ? file_dir
                                                : file_none;
    node->size = file0->type == file_dir ? 0 : file0->size;
    return EOK;
}

errno_t tmpfs_delete(void *parent, vfs_node_t node) {
    tmpfs_file_t *f = (tmpfs_file_t *)node->handle;
    if (f == NULL)
        return EOK;
    if (f->link_count > 1) {
        f->link_count--;
        return EOK;
    }
    if (f->data != NULL)
        free(f->data);
    free(f);
    return EOK;
}

void tmpfs_open(void *parent, const char *name, vfs_node_t node) {
}

errno_t tmpfs_rename(void *current, const char *new_name) {
    tmpfs_file_t *f = (tmpfs_file_t *)current;
    strncpy(f->name, new_name, sizeof(f->name));
    return EOK;
}

int tmpfs_poll(void *file, size_t events) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;
    int revents     = 0;
    if (events & POLLIN)
        revents |= POLLIN;
    if (events & POLLOUT)
        revents |= POLLOUT;
    return revents;
}

bool tmpfs_close(void *file) {
    return false;
}

void *tmpfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map(tmpfs_read, file, (uint64_t)addr, size, prot, flags, offset);
}

vfs_node_t tmpfs_dup(vfs_node_t node) {
    vfs_node_t copy    = vfs_node_alloc(node->parent, node->name);
    tmpfs_file_t *file = node->handle;
    if (file != NULL)
        file->link_count++;
    copy->handle      = node->handle;
    copy->type        = node->type;
    copy->size        = node->size;
    copy->linkname    = node->linkname == NULL ? NULL : strdup(node->linkname);
    copy->flags       = node->flags;
    copy->permissions = node->permissions;
    copy->owner       = node->owner;
    copy->child       = node->child;
    copy->realsize    = node->realsize;
    copy->inode       = node->inode;
    return copy;
}

errno_t tmpfs_symlink(void *parent, const char *name, vfs_node_t node) {
    tmpfs_file_t *p = parent;
    tmpfs_file_t *f = calloc(1, sizeof(tmpfs_file_t));
    strncpy(f->name, name, sizeof(f->name));
    f->type       = tp_file_symlink;
    f->link_count = 1;
    node->handle  = f;
    f->node       = node;
    return EOK;
}

size_t tmpfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    if (node == NULL || addr == NULL || size == 0)
        return 0;
    const char *target = node->linkto_path;
    if (target == NULL && node->linkto != NULL) {
        target = vfs_get_fullpath(node->linkto);
        if (target == NULL)
            return 0;
    }
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

errno_t tmpfs_free(void *handle) {
    if (handle == NULL)
        return EOK;
    tmpfs_file_t *file = handle;
    if (file->link_count > 1) {
        file->link_count--;
        return EOK;
    }
    if (file->type == tp_file_file && file->data != NULL)
        free(file->data);
    free(file);
    return EOK;
}

errno_t tmpfs_chmod(vfs_node_t node, uint16_t mode) {
    node->mode = mode;
    return EOK;
}

errno_t tmpfs_mknod(void *parent, const char *name, vfs_node_t node, uint16_t mode, int dev) {
    node->dev            = dev;
    node->rdev           = dev;
    node->mode           = mode & 0777;
    tmpfs_file_t *handle = calloc(1, sizeof(tmpfs_file_t));
    handle->size         = 0;
    handle->link_count   = 1;
    handle->node         = node;
    if ((mode & S_IFMT) == S_IFBLK) {
        node->type   = file_block;
        handle->type = tp_file_blk;
    }
    if ((mode & S_IFMT) == S_IFCHR) {
        node->type   = file_stream;
        handle->type = tp_file_char;
    } else {
        node->type   = file_none;
        handle->type = tp_file_file;
    }
    strncpy(handle->name, name, 64);
    node->handle = handle;
    return 0;
}

errno_t tmpfs_ioctl(void *file, size_t req, void *arg) {
    tmpfs_file_t *handle = file;
    if (handle->type == tp_file_char || handle->type == tp_file_blk) {
        return EOK;
    }
    return EOK;
}

static struct vfs_callback tmpfs_callbacks = {
    .mount    = tmpfs_mount,
    .unmount  = tmpfs_umount,
    .mkdir    = tmpfs_mkdir,
    .close    = tmpfs_close,
    .stat     = tmpfs_stat,
    .open     = tmpfs_open,
    .read     = tmpfs_read,
    .write    = tmpfs_write,
    .readlink = tmpfs_readlink,
    .mkfile   = tmpfs_mkfile,
    .link     = tmpfs_link,
    .symlink  = tmpfs_symlink,
    .ioctl    = tmpfs_ioctl,
    .dup      = tmpfs_dup,
    .delete   = tmpfs_delete,
    .rename   = tmpfs_rename,
    .poll     = tmpfs_poll,
    .map      = tmpfs_map,
    .free     = tmpfs_free,
    .chmod    = tmpfs_chmod,
    .mknod    = tmpfs_mknod,
};

void tmpfs_regist() {
    tmpfs_id = vfs_regist("tmpfs", &tmpfs_callbacks, 0x01021994, FS_VIRTUAL_FLAGS);
    if (tmpfs_id & ERRNO_MASK) {
        kerror("tmpfs register error");
    }
}
