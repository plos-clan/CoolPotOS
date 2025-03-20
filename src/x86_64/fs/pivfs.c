#define ALL_IMPLEMENTATION
#include "pivfs.h"
#include "kprint.h"
#include "krlibc.h"
#include "list.h"
#include "lock.h"
#include "sprintf.h"
#include "vfs.h"

static int        pivfs_id   = 0;
static vfs_node_t pivfs_root = NULL;

static pivfs_handle_t *file_alloc(size_t size) {
    pivfs_handle_t *handle = malloc(sizeof(pivfs_handle_t));
    handle->size           = size;
    handle->type           = FILE;
    handle->blocks         = malloc(size);
    return handle;
}

static void file_free(pivfs_handle_t *handle) {
    free(handle->blocks);
    free(handle);
}

static int pivfs_mount(const char *handle, vfs_node_t node) {
    if (handle != (const char *)1) return VFS_STATUS_FAILED;
    if (pivfs_root) {
        kerror("Process file system has been mounted.");
        return VFS_STATUS_FAILED;
    }
    node->fsid = pivfs_id;
    pivfs_root = node;
    return VFS_STATUS_SUCCESS;
}

static int pivfs_mkdir(void *handle0, const char *name, vfs_node_t node) {
    pivfs_handle_t *handle   = handle0;
    char           *new_path = malloc(strlen(handle->path) + strlen(name) + 1 + 1);
    sprintf(new_path, "%s/%s", handle->path, name);
    pivfs_handle_t *new_dir = malloc(sizeof(pivfs_handle_t));
    new_dir->type           = DIR;
    new_dir->path           = new_path;
    new_dir->blocks         = NULL;
    new_dir->size           = 0;
    new_dir->child          = list_alloc(NULL);
    node->handle            = new_dir;
    list_append(handle->child, new_dir);
    return VFS_STATUS_SUCCESS;
}

static int pivfs_stat(void *handle0, vfs_node_t node) {
    pivfs_handle_t *handle = handle0;
    printk("stats: \n");
    if (handle == NULL) return VFS_STATUS_FAILED;
    if (handle->type == DIR) {
        node->type = file_dir;
    } else {
        node->type = file_block;
        node->size = handle->size;
    }
    return VFS_STATUS_SUCCESS;
}

static int pivfs_mkfile(void *handle0, const char *name, vfs_node_t node) {
    pivfs_handle_t *handle   = handle0;
    char           *new_path = malloc(strlen(handle->path) + strlen(name) + 1 + 1);
    sprintf(new_path, "%s/%s", handle->path, name);
    pivfs_handle_t *new_file = file_alloc(10);
    new_file->path           = new_path;
    new_file->child          = NULL;
    node->handle             = new_file;
    list_append(handle->child, new_file);
    return VFS_STATUS_SUCCESS;
}

struct vfs_callback pivfs_callbacks = {
    .mount   = pivfs_mount,
    .unmount = (void *)empty,
    .mkdir   = pivfs_mkdir,
    .stat    = pivfs_stat,
    .mkfile  = pivfs_mkfile,

    .read  = (void *)empty,
    .write = (void *)empty,
    .open  = (void *)empty,
    .close = (void *)empty,
};

void pivfs_setup() {
    pivfs_id = vfs_regist("pivfs", &pivfs_callbacks);
    vfs_mkdir("/proc");
    vfs_node_t proc = vfs_open("/proc");
    if (proc == NULL) {
        printk("'proc' handle is null.\n");
        return;
    }
    if (vfs_mount((const char *)1, proc) == VFS_STATUS_FAILED) {
        kerror("Cannot mount process file system.");
    } else {
        pivfs_handle_t *handle = file_alloc(1);
        handle->type           = DIR;
        handle->path           = malloc(2);
        handle->child          = list_alloc(handle);
        handle->blocks         = NULL;
        handle->size           = 0;
        strcpy(handle->path, "/");
        pivfs_root->handle = handle;
    }
}

static void update_process(char *path, pcb_t pcb, bool is_build) {
    char buf[50];
    sprintf(buf, "%s/name", path);

    if (is_build) { vfs_mkfile(buf); }
}

void pivfs_update(pcb_t kernel_head) {
    if (kernel_head == NULL || pivfs_root == NULL) return;
    //    pcb_t f = kernel_head;
    //    do{
    //        char buf[100];
    //        sprintf(buf,"/proc/%d",f->pid);
    //        vfs_node_t node = vfs_open(buf);
    //        vfs_mkdir(buf);
    //        if(node == NULL)
    //            update_process(buf,f,true);
    //        else update_process(buf,f,false);
    //        f = f->next;
    //    } while (f != NULL && f->pid != kernel_head->pid);
}
