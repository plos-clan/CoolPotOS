/**
 * Module Virtual File System
 * 模块虚拟文件系统
 */
#define ALL_IMPLEMENTATION
#include "modfs.h"
#include "errno.h"
#include "kprint.h"
#include "krlibc.h"
#include "module.h"
#include "rbtree-strptr.h"
#include "vfs.h"

extern int         module_count;
int                modfs_id   = 0;
static vfs_node_t  modfs_root = NULL;
static rbtree_sp_t mod_rbtree;
extern cp_module_t module_ls[256]; // vdisk.c

static int modfs_mkdir(void *handle, const char *name, vfs_node_t node) {
    node->fsid = 0;
    return 0;
}

static int modfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (void *)MODFS_REGISTER_ID) return VFS_STATUS_FAILED;
    if (modfs_root) {
        kerror("Module file system has been mounted.");
        return VFS_STATUS_FAILED;
    }
    node->fsid = modfs_id;
    modfs_root = node;
    for (int i = 0; i < module_count; i++) {
        vfs_child_append(modfs_root, module_ls[i].module_name, NULL);
        rbtree_sp_insert(mod_rbtree, module_ls[i].module_name, (void *)&module_ls[i]);
    }
    return VFS_STATUS_SUCCESS;
}

static int modfs_stat(void *handle, vfs_node_t node) {
    if (node->type == file_dir) return VFS_STATUS_SUCCESS;
    cp_module_t *mod = rbtree_sp_get(mod_rbtree, node->name);
    node->handle     = mod;
    node->type       = file_block;
    node->size       = mod->size;
    return VFS_STATUS_SUCCESS;
}

static void modfs_open(void *parent, const char *name, vfs_node_t node) {
    if (node->type == file_dir) return;
    cp_module_t *mod = rbtree_sp_get(mod_rbtree, name);
    node->handle     = mod;
    node->type       = file_block;
    node->size       = mod->size;
    node->refcount   = 1;
}

static size_t modfs_read(void *file, void *addr, size_t offset, size_t size) {
    if (file == NULL) return VFS_STATUS_FAILED;
    cp_module_t *mod = (cp_module_t *)file;
    if (offset > mod->size) { return VFS_STATUS_FAILED; }
    void *buffer = mod->data + offset;
    memcpy(addr, buffer, (size + offset) > mod->size ? size : mod->size - offset);
    return size;
}

static size_t modfs_write(void *file, const void *addr, size_t offset, size_t size) {
    return VFS_STATUS_FAILED;
}

static vfs_node_t modfs_dup(vfs_node_t node) {
    vfs_node_t new_node = vfs_node_alloc(node->parent, node->name);
    if (new_node == NULL) return NULL;
    new_node->type        = node->type;
    new_node->handle      = node->handle;
    new_node->size        = node->size;
    new_node->child       = node->child;
    new_node->flags       = node->flags;
    new_node->permissions = node->permissions;
    new_node->realsize    = node->realsize;
    return new_node;
}

static int dummy() {
    return -ENOSYS;
}

static struct vfs_callback modfs_callbacks = {
    .mount    = modfs_mount,
    .unmount  = (void *)empty,
    .mkdir    = modfs_mkdir,
    .close    = (void *)empty,
    .stat     = modfs_stat,
    .open     = modfs_open,
    .read     = modfs_read,
    .write    = modfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = (void *)empty,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .ioctl    = (void *)empty,
    .dup      = modfs_dup,
    .delete   = (void *)empty,
    .rename   = (void *)empty,
    .poll     = (void *)empty,
    .map      = (void *)empty,
};

void modfs_setup() {
    modfs_id = vfs_regist("modfs", &modfs_callbacks, MODFS_REGISTER_ID, 0x858458f6);
}
