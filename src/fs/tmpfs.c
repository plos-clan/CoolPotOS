#include "fs/tmpfs.h"
#include "errno.h"
#include "mem/frame.h"
#include "krlibc.h"
#include "mem/page.h"
#include "task/poll.h"
#include "term/klog.h"

static int tmpfs_id              = 0;
static _Atomic int mount_dev_now = 0;

static bool tmpfs_size_to_pages(size_t size, size_t *pages_out) {
    if (pages_out == NULL)
        return false;
    if (size == 0) {
        *pages_out = 0;
        return true;
    }
    if (size > (size_t)-1 - (PAGE_SIZE - 1))
        return false;
    *pages_out = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    return true;
}

static void tmpfs_release_pages(tmpfs_file_t *file) {
    if (file == NULL)
        return;
    if (file->data != NULL && file->page_num != 0) {
        free_frames(virt_to_phys(file->data), file->page_num);
    }
    file->data     = NULL;
    file->page_num = 0;
    file->capacity = 0;
}

static bool tmpfs_ensure_capacity(tmpfs_file_t *file, size_t end) {
    if (file == NULL)
        return false;
    if (end <= file->capacity)
        return true;

    size_t need_pages = 0;
    if (!tmpfs_size_to_pages(end, &need_pages) || need_pages == 0)
        return false;

    size_t new_pages = file->page_num != 0 ? file->page_num : 1;
    while (new_pages < need_pages) {
        if (new_pages > (size_t)-1 / 2) {
            new_pages = need_pages;
            break;
        }
        new_pages *= 2;
    }

    if (new_pages > (size_t)-1 / PAGE_SIZE)
        return false;

    uint64_t phys = alloc_frames(new_pages);
    if (phys == 0)
        return false;

    char *new_data = phys_to_virt(phys);
    if (new_data == NULL) {
        free_frames(phys, new_pages);
        return false;
    }

    size_t new_capacity = new_pages * PAGE_SIZE;
    memset(new_data, 0, new_capacity);
    if (file->data != NULL && file->size != 0) {
        memcpy(new_data, file->data, file->size);
    }

    tmpfs_release_pages(file);
    file->data     = new_data;
    file->page_num = new_pages;
    file->capacity = new_capacity;
    return true;
}

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
    if (unlikely(size == 0)) {
        return 0;
    }
    tmpfs_file_t *f = file;
    size_t end      = 0;

    if (__builtin_add_overflow(offset, size, &end))
        return 0;

    if (!tmpfs_ensure_capacity(f, end)) {
        return 0;
    }

    if (offset > f->size) {
        memset(f->data + f->size, 0, offset - f->size);
    }
    memcpy(f->data + offset, addr, size);
    if (end > f->size)
        f->size = end;
    f->node->size = f->size;
    return size;
}

errno_t tmpfs_stat(void *file, vfs_node_t node) {
    const tmpfs_file_t *file0 = file;
    if (file0 == NULL) {
        return -ENOENT;
    }
    node->type = file0->type == tp_file_symlink  ? file_symlink
                 : file0->type == tp_file_dir    ? file_dir
                 : file0->type == tp_file_blk    ? file_block
                 : file0->type == tp_file_char   ? file_stream
                 : file0->type == tp_file_socket ? file_socket
                                                 : file_none;
    node->size = file0->type == file_dir ? 0 : file0->size;
    return EOK;
}

errno_t tmpfs_delete(void *parent, vfs_node_t node) {
    tmpfs_file_t *f = node->handle;
    if (f == NULL) {
        return EOK;
    }
    if (f->link_count > 1) {
        f->link_count--;
        return EOK;
    }
    tmpfs_release_pages(f);
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
    (tmpfs_file_t *)file;
    int revents = 0;
    if (events & POLLIN) {
        revents |= POLLIN;
    }
    if (events & POLLOUT) {
        revents |= POLLOUT;
    }
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
    if (file != NULL) {
        file->link_count++;
    }
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
    if (file->type == tp_file_file)
        tmpfs_release_pages(file);
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
    } else if ((mode & S_IFMT) == S_IFCHR) {
        node->type   = file_stream;
        handle->type = tp_file_char;
    } else if ((mode & S_IFMT) == S_IFSOCK) {
        node->type   = file_socket;
        handle->type = tp_file_socket;
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
