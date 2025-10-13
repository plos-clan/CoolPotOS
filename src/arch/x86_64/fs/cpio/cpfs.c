#include "cpfs.h"
#include "errno.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "poll.h"
#include "vfs.h"

int        cpfs_id       = 0;
vfs_node_t cpfs_root_vfs = NULL;

static cpfs_file_t *cpfs_root;

errno_t cpfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (void *)CPFS_REGISTER_ID) return VFS_STATUS_FAILED;
    node->fsid        = cpfs_id;
    cpfs_root_vfs     = node;
    cpfs_root         = (cpfs_file_t *)malloc(sizeof(cpfs_file_t));
    cpfs_root->is_dir = true;
    strcpy(cpfs_root->name, "");
    cpfs_root->parent      = NULL;
    cpfs_root->child_count = 0;
    cpfs_root->ready       = true;
    llist_init_head(&cpfs_root->child_node);
    llist_init_head(&cpfs_root->curr_node);
    node->handle = cpfs_root;
    node->dev    = 0;
    return VFS_STATUS_SUCCESS;
}

void cpfs_open(void *parent, const char *name, vfs_node_t node) {
    cpfs_file_t *p = (cpfs_file_t *)parent;
    cpfs_file_t *pos, *nxt;
    llist_for_each(pos, nxt, &p->child_node, curr_node) {
        if (strcmp(pos->name, name) == 0) {
            pos->ready   = false;
            node->handle = pos;
            node->fsid   = cpfs_id;
            node->type   = pos->is_dir ? file_dir : file_none;
            node->size   = pos->is_dir ? 0 : pos->size;
            if (pos->is_symlink) node->type |= file_symlink;
            node->refcount = 1;
            return;
        }
    }
failed:
    node->handle = NULL; // 没找到
}

size_t cpfs_read(void *file, void *addr, size_t offset, size_t size) {
    cpfs_file_t *f = (cpfs_file_t *)file;
    if (offset >= f->size) return 0;
    size_t actual = (offset + size > f->size) ? (f->size - offset) : size;
    memcpy(addr, f->data + offset, actual);
    return actual;
}

size_t cpfs_write(void *file, const void *addr, size_t offset, size_t size) {
    cpfs_file_t *f   = (cpfs_file_t *)file;
    size_t       end = offset + size;
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

errno_t cpfs_mk(void *parent, const char *name, vfs_node_t node, bool is_dir) {
    cpfs_file_t *p = (cpfs_file_t *)parent;
    cpfs_file_t *f = calloc(1, sizeof(cpfs_file_t));
    strncpy(f->name, name, sizeof(f->name));
    f->is_dir     = is_dir;
    f->is_symlink = false;
    f->parent     = p;
    f->ready      = true;
    f->linkto     = NULL;
    llist_init_head(&f->child_node);
    llist_init_head(&f->curr_node);
    llist_append(&p->child_node, &f->curr_node);
    return VFS_STATUS_SUCCESS;
}

errno_t cpfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    return cpfs_mk(parent, name, node, true);
}

errno_t cpfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    return cpfs_mk(parent, name, node, false);
}

errno_t cpfs_stat(void *file, vfs_node_t node) {
    cpfs_file_t *file0 = (cpfs_file_t *)file;
    if (file0 == NULL) return VFS_STATUS_FAILED;
    node->type = file0->is_symlink ? file_symlink : (file0->is_dir ? file_dir : file_none);
    node->size = file0->is_dir ? 0 : file0->size;
    return VFS_STATUS_SUCCESS;
}

errno_t cpfs_delete(void *parent, vfs_node_t node) {
    cpfs_file_t *p = (cpfs_file_t *)parent;
    cpfs_file_t *f = (cpfs_file_t *)node->handle;
    llist_delete(&f->curr_node);
    return -ENOENT;
}

errno_t cpfs_rename(void *current, const char *new_name) {
    cpfs_file_t *f = (cpfs_file_t *)current;
    strncpy(f->name, new_name, sizeof(f->name));
    return EOK;
}

vfs_node_t cpfs_dup(vfs_node_t node) {
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

int cpfs_poll(void *file, size_t events) {
    cpfs_file_t *f = (cpfs_file_t *)file;
    if (f->ready) {
        int revents = 0;
        if (events & POLLIN) revents |= POLLIN;
        if (events & POLLOUT) revents |= POLLOUT;
        return revents;
    }
    return 0;
}

void cpfs_close(void *file) {
    cpfs_file_t *f = (cpfs_file_t *)file;
    f->ready       = true;
}

void *cpfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map(cpfs_read, file, (uint64_t)addr, offset, size, prot, flags);
}

errno_t cpfs_symlink(void *parent, const char *name, vfs_node_t node) {
    cpfs_file_t *p = parent;
    cpfs_file_t *f = calloc(1, sizeof(cpfs_file_t));
    strncpy(f->name, name, sizeof(f->name));
    f->is_dir     = false;
    f->is_symlink = true;
    f->parent     = p;
    f->ready      = true;
    llist_init_head(&f->child_node);
    llist_init_head(&f->curr_node);
    llist_append(&p->child_node, &f->curr_node);
    node->handle = f;
    return EOK;
}

static void free_child_node(cpfs_file_t *parent, cpfs_file_t *dir) {
    if (dir->is_dir) {
        do {
            cpfs_file_t *tmp = NULL;
            cpfs_file_t *pos = NULL;
            cpfs_file_t *n = NULL;
            llist_for_each(pos, n, &dir->child_node, curr_node) {
                tmp = pos;
            }
            if (tmp == NULL) break;
            free_child_node(dir, tmp);
            free(dir);
        } while (true);
        return;
    }
    llist_delete(&dir->curr_node);
    free(dir->data);
    free(dir);
}

void cpfs_unmount(void *root) {
    cpfs_file_t *handle = root;
    free_child_node(NULL, handle);
}

static int dummy() {
    return -ENOSYS;
}

static struct vfs_callback cpfs_callbacks = {
    .mount    = cpfs_mount,
    .unmount  = cpfs_unmount,
    .mkdir    = cpfs_mkdir,
    .close    = cpfs_close,
    .stat     = cpfs_stat,
    .open     = cpfs_open,
    .read     = cpfs_read,
    .write    = cpfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = cpfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = cpfs_symlink,
    .ioctl    = (void *)empty,
    .dup      = cpfs_dup,
    .delete   = cpfs_delete,
    .rename   = cpfs_rename,
    .poll     = cpfs_poll,
    .map      = cpfs_map,
};

void cpfs_setup() {
    cpfs_id = vfs_regist("cpfs", &cpfs_callbacks, CPFS_REGISTER_ID, 0x858458f6);
}
