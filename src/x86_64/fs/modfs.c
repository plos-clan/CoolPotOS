/**
 * Module Virtual File System
 * 模块虚拟文件系统
 */
#define ALL_IMPLEMENTATION
#include "modfs.h"
#include "module.h"
#include "vfs.h"
#include "kprint.h"
#include "krlibc.h"
#include "rbtree-strptr.h"

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
    if (handle != (void*)1) return VFS_STATUS_FAILED;
    if (modfs_root) {
        kerror("Module file system has been mounted.");
        return VFS_STATUS_FAILED;
    }
    node->fsid = modfs_id;
    modfs_root = node;
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
}

static int modfs_read(void *file, void *addr, size_t offset, size_t size) {
    if(file == NULL) return VFS_STATUS_FAILED;
    cp_module_t *mod = (cp_module_t *)file;
    if (offset + size > mod->size) {
        return VFS_STATUS_FAILED;
    }
    void *buffer = mod->data + offset;
    memcpy(addr, buffer, size);
    return VFS_STATUS_SUCCESS;
}

static struct vfs_callback modfs_callbacks = {
        .mount   = modfs_mount,
        .unmount = (void *)empty,
        .mkdir   = modfs_mkdir,
        .close   = (void *)empty,
        .stat    = modfs_stat,
        .open    = modfs_open,
        .read    = modfs_read,
        .write   = (void*)empty,
        .mkfile  = (void *)empty,
};

void modfs_setup(){
    modfs_id = vfs_regist("modfs", &modfs_callbacks);
    vfs_mkdir("/mod");
    vfs_node_t mod = vfs_open("/mod");
    if (mod == NULL) {
        kerror("'mod' handle is null.");
        return;
    }
    if (vfs_mount((const char *) 1, mod) == VFS_STATUS_FAILED) {
        kerror("Cannot mount module file system.");
        return;
    }

    for (int i = 0; i < module_count; i++) {
        vfs_child_append(modfs_root, module_ls[i].module_name, NULL);
        rbtree_sp_insert(mod_rbtree, module_ls[i].module_name, (void *)&module_ls[i]);
    }
}
