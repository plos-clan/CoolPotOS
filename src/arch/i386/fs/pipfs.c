#include "pipfs.h"
#include "klog.h"
#include "scheduler.h"

#define ALL_IMPLEMENTATION
#define SLIST_SP_IMPLEMENTATION
#include "rbtree-strptr.h"

int           pipfs_id = 0;
vfs_node_t    pip_fs_node;
rbtree_sp_t   pip_rbtree;
extern pcb_t *running_proc_head;

static file_t file_alloc(size_t size) {
    file_t file = kmalloc(sizeof(struct file));
    if (file == NULL) return NULL;
    file->size   = size;
    file->blocks = kmalloc(size);
    memset(file->blocks, 0, size);
    return file;
}

static void file_free(file_t file) {
    if (file == NULL) return;
    kfree(file->blocks);
    kfree(file);
}

int pipfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    node->handle = NULL;
    return 0;
}

int pipfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    file_t file = file_alloc(node->size);
    if (file == NULL) return -1;
    node->handle = file;
    return 0;
}

void pipfs_close(file_t handle) {
    file_free(handle);
}

size_t pipfs_readfile(file_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    for (size_t i = 0; i <= file->size; i++) {
        file->blocks[i] = ((uint8_t *)addr)[i];
    }
    return 0;
}

int pipfs_writefile(file_t file, const void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    if (get_current_proc()->task_level != TASK_KERNEL_LEVEL) return -1;
    for (size_t i = 0; i <= file->size; i++) {
        ((uint8_t *)addr)[i] = file->blocks[i];
    }
    return 0;
}

void pipfs_open(void *parent, const char *name, vfs_node_t node) {
    node->handle = rbtree_sp_get(pip_rbtree, name);
    node->type   = file_block;
    node->size   = sizeof(pcb_t);
}

int pipfs_mount(const char *src, vfs_node_t node) {
    if (src != 1) return -1;
    node->fsid = pipfs_id;

    pipfs_update();

    return -1;
}

void pipfs_unmount(void *root) {
    //    //
}

int pipfs_stat(void *file, vfs_node_t node) {
    if (node->type == file_dir) return 0;
    node->handle = rbtree_sp_get(pip_rbtree, node->name);
    node->type   = file_block;
    node->size   = sizeof(pcb_t);
}

static struct vfs_callback callbacks = {
    .mount   = pipfs_mount,
    .unmount = pipfs_unmount,
    .open    = pipfs_open,
    .close   = (vfs_close_t)pipfs_close,
    .read    = (vfs_read_t)pipfs_readfile,
    .write   = (vfs_write_t)pipfs_writefile,
    .mkdir   = pipfs_mkdir,
    .mkfile  = pipfs_mkfile,
    .stat    = pipfs_stat,
};

void pipfs_update() {
    if (pip_fs_node != NULL) rbtree_sp_free(pip_rbtree);
    pip_fs_node = NULL;
    pcb_t *l    = running_proc_head;
    loop {
        rbtree_sp_insert(pip_rbtree, l->name, l);
        l = l->next;
        if (l == NULL || l == running_proc_head) break;
    }
}

void pipfs_regist() {
    // TODO pipfs
    return;
    pipfs_id = vfs_regist("pipfs", &callbacks);
    vfs_mkdir("/proc");
    pip_fs_node = vfs_open("/proc");
    vfs_mount((void *)1, pip_fs_node);

    klogf(true, "Process File System initialize.\n");
}
