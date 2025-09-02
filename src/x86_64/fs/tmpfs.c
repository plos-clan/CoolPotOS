#include "tmpfs.h"
#include "errno.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "poll.h"
#include "vfs.h"

int        tmpfs_id       = 0;
vfs_node_t tmpfs_root_vfs = NULL;

static tmpfs_file_t *tmpfs_root;

errno_t tmpfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (void *)TMPFS_REGISTER_ID) return VFS_STATUS_FAILED;
    node->fsid         = tmpfs_id;
    tmpfs_root_vfs     = node;
    tmpfs_root         = (tmpfs_file_t *)malloc(sizeof(tmpfs_file_t));
    tmpfs_root->is_dir = true;
    strcpy(tmpfs_root->name, "tmp");
    tmpfs_root->parent      = NULL;
    tmpfs_root->child_count = 0;
    tmpfs_root->ready       = true;
    node->handle            = tmpfs_root;
    return VFS_STATUS_SUCCESS;
}

void tmpfs_open(void *parent, const char *name, vfs_node_t node) {
    tmpfs_file_t *p = (tmpfs_file_t *)parent;
    for (size_t i = 0; i < p->child_count; i++) {
        if (strcmp(p->children[i]->name, name) == 0) {
            tmpfs_file_t *file = p->children[i];
            if (!file->ready) goto failed;
            file->ready    = false;
            node->handle   = file;
            node->fsid     = tmpfs_id;
            node->type     = file->is_dir ? file_dir : file_none;
            node->refcount = 1;
            return;
        }
    }
failed:
    node->handle = NULL; // 没找到
}

size_t tmpfs_read(void *file, void *addr, size_t offset, size_t size) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;
    if (offset >= f->size) return 0;
    size_t actual = (offset + size > f->size) ? (f->size - offset) : size;
    memcpy(addr, f->data + offset, actual);
    return actual;
}

size_t tmpfs_write(void *file, const void *addr, size_t offset, size_t size) {
    tmpfs_file_t *f   = (tmpfs_file_t *)file;
    size_t        end = offset + size;
    if (end > f->capacity) {
        size_t new_cap = end * 2;
        char  *new_buf = realloc(f->data, new_cap);
        if (!new_buf) return 0;
        f->data     = new_buf;
        f->capacity = new_cap;
    }
    memcpy(f->data + offset, addr, size);
    if (end > f->size) f->size = end;
    return size;
}

errno_t tmpfs_mk(void *parent, const char *name, vfs_node_t node, bool is_dir) {
    tmpfs_file_t *p = (tmpfs_file_t *)parent;
    if (p->child_count >= 64) return VFS_STATUS_FAILED;
    tmpfs_file_t *f = calloc(1, sizeof(tmpfs_file_t));
    strncpy(f->name, name, sizeof(f->name));
    f->is_dir                     = is_dir;
    f->parent                     = p;
    f->ready                      = true;
    p->children[p->child_count++] = f;
    // node->handle                  = f;
    return VFS_STATUS_SUCCESS;
}

errno_t tmpfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    return tmpfs_mk(parent, name, node, true);
}

errno_t tmpfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    return tmpfs_mk(parent, name, node, false);
}

errno_t tmpfs_stat(void *file, vfs_node_t node) {
    tmpfs_file_t *file0 = (tmpfs_file_t *)file;
    if (file0 == NULL) return VFS_STATUS_FAILED;
    node->type = file0->is_dir ? file_dir : file_none;
    node->size = file0->is_dir ? 0 : file0->size;
    return VFS_STATUS_SUCCESS;
}

errno_t tmpfs_delete(void *parent, vfs_node_t node) {
    tmpfs_file_t *p = (tmpfs_file_t *)parent;
    tmpfs_file_t *f = (tmpfs_file_t *)node->handle;
    for (size_t i = 0; i < p->child_count; i++) {
        if (p->children[i] == f) {
            memmove(&p->children[i], &p->children[i + 1],
                    (p->child_count - i - 1) * sizeof(void *));
            p->child_count--;
            free(f->data);
            free(f);
            return EOK;
        }
    }
    return -ENOENT;
}

errno_t tmpfs_rename(void *current, const char *new_name) {
    tmpfs_file_t *f = (tmpfs_file_t *)current;
    strncpy(f->name, new_name, sizeof(f->name));
    return EOK;
}

vfs_node_t tmpfs_dup(vfs_node_t node) {
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

int tmpfs_poll(void *file, size_t events) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;
    if (f->ready) {
        int revents = 0;
        if (events & POLLIN) revents |= POLLIN;
        if (events & POLLOUT) revents |= POLLOUT;
        return revents;
    } else
        return 0;
}

void tmpfs_close(void *file) {
    tmpfs_file_t *f = (tmpfs_file_t *)file;
    f->ready        = true;
}

void *tmpfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map(tmpfs_read, file, (uint64_t)addr, offset, size, prot, flags);
}

static int dummy() {
    return -ENOSYS;
}

static struct vfs_callback tmpfs_callbacks = {
    .mount    = tmpfs_mount,
    .unmount  = (void *)empty,
    .mkdir    = tmpfs_mkdir,
    .close    = tmpfs_close,
    .stat     = tmpfs_stat,
    .open     = tmpfs_open,
    .read     = tmpfs_read,
    .write    = tmpfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = tmpfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .ioctl    = (void *)empty,
    .dup      = tmpfs_dup,
    .delete   = tmpfs_delete,
    .rename   = tmpfs_rename,
    .poll     = tmpfs_poll,
    .map      = tmpfs_map,
};

void tmpfs_setup() {
    tmpfs_id = vfs_regist("tmpfs", &tmpfs_callbacks);
    vfs_mkdir("/tmp");
    vfs_node_t dev = vfs_open("/tmp");
    if (dev == NULL) {
        kerror("'tmp' handle is null.");
        return;
    }
    if (vfs_mount((const char *)TMPFS_REGISTER_ID, dev) == VFS_STATUS_FAILED) {
        kerror("Cannot mount temp file system.");
    }
}
