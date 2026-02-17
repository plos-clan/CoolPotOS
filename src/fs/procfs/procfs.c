#define ALL_IMPLEMENTATION
#include "fs/procfs.h"
#include "bootarg.h"
#include "errno.h"
#include "fs/fds.h"
#include "intctl.h"
#include "mem/vma.h"
#include "task/smp.h"
#include "term/klog.h"

static int procfs_id    = 0;
static int proc_self_id = 0;
vfs_node_t procfs_root  = NULL;
spin_t procfs_oplock    = SPIN_INIT;

extern cow_arraylist *process_list;

errno_t procfs_mount(const char *src, vfs_node_t node) {
    procfs_root = node;

    procfs_root->fsid = procfs_id;

    vfs_node_t procfs_self = vfs_node_alloc(procfs_root, "self");
    procfs_self->type      = file_symlink;
    procfs_self->mode      = 0644;
    procfs_self->linkto    = NULL;
    procfs_self->handle    = NULL;
    procfs_self->fsid      = proc_self_id;

    load_procfs_root();

    pcb_t Inode = NULL;
    cow_foreach(process_list, Inode) {
        procfs_on_new_task(Inode);
    }

    return EOK;
}

void procfs_open(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent, name, node);
}

bool procfs_close(void *current) {
    UNUSED(current);
    return false;
}

size_t procfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    UNUSED(node, addr, offset, size);
    return 0;
}

size_t procfs_write(void *file, const void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return size;
}

size_t procfs_read(void *file, void *addr, size_t offset, size_t size) {
    proc_handle_t *handle = (proc_handle_t *)file;
    if (!handle) {
        return -1;
    }
    return procfs_read_dispatch(handle, addr, offset, size);
}

vfs_node_t procfs_dup(vfs_node_t src) {
    return src;
}

errno_t procfs_stat(void *file, vfs_node_t node) {
    if (file == NULL)
        return EOK;
    proc_handle_t *handle = file;
    procfs_stat_dispatch(handle, node);
    return EOK;
}

void procfs_self_open(void *parent, const char *name, vfs_node_t node) {
    procfs_self_handle_t *handle = malloc(sizeof(procfs_self_handle_t));
    handle->self                 = node;
    node->linkto                 = get_current_task()->process->procfs_node;
    node->handle                 = handle;
    vfs_node_t new_self_node     = vfs_node_alloc(node->parent, "self");
    new_self_node->type          = file_symlink;
    new_self_node->mode          = 0644;
    new_self_node->fsid          = proc_self_id;
    list_delete(node->parent->child, node);
}

bool procfs_self_close(void *current) {
    procfs_self_handle_t *handle = current;
    handle->self->type |= file_delete;
    free(handle);
    return true;
}

size_t procfs_self_read(void *fd, void *addr, size_t offset, size_t size) {
    procfs_self_handle_t *handle = fd;
    return procfs_read(handle->self->linkto->handle, addr, offset, size);
}

size_t procfs_self_write(void *fd, const void *addr, size_t offset, size_t size) {
    procfs_self_handle_t *handle = fd;
    return procfs_write(handle->self->linkto->handle, addr, offset, size);
}

size_t procfs_self_readlink(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (!(file->type & file_symlink))
        return 0;
    if (offset >= strlen(file->linkto->name))
        return 0;
    logkf("procfs: readlink offset:%llu size:%llu", offset, size);
    char *ptr   = file->linkto->name + offset;
    ssize_t len = strlen(ptr);
    len         = MIN(len, (ssize_t)size);
    memcpy(addr, ptr, len);
    return len;
}

errno_t procfs_self_stat(void *file, vfs_node_t node) {
    procfs_self_handle_t *handle = file;
    node->type |= file_symlink;
    node->linkto = get_current_task()->process->procfs_node;
    node->size   = strlen(node->linkto->name);
    return EOK;
}

void procfs_self_free_handle(procfs_self_handle_t *handle) {
    free(handle);
}

errno_t procfs_free(void *handle) {
    proc_handle_t *file = handle;
    free(file);
    return EOK;
}

errno_t procfs_self_free(void *handle) {
    procfs_self_handle_t *file = handle;
    free(file);
    return EOK;
}

static struct vfs_callback procfs_self_callbacks = {
    .open     = procfs_self_open,
    .close    = procfs_self_close,
    .read     = procfs_self_read,
    .write    = procfs_self_write,
    .readlink = procfs_self_readlink,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .stat     = procfs_self_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .poll     = (vfs_poll_t)dummy,
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .dup      = (vfs_dup_t)dummy,
    .free     = (vfs_free_t)procfs_self_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

static struct vfs_callback procfs_callbacks = {
    .mount    = procfs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .close    = procfs_close,
    .stat     = procfs_stat,
    .open     = procfs_open,
    .read     = procfs_read,
    .write    = procfs_write,
    .readlink = procfs_readlink,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .ioctl    = (vfs_ioctl_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .poll     = (vfs_poll_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .dup      = procfs_dup,
    .free     = (vfs_free_t)procfs_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void procfs_setup() {
    procfs_id = vfs_regist("proc", &procfs_callbacks, 0x9fa0, FS_VIRTUAL_FLAGS);
    proc_self_id =
        vfs_regist("proc_self", &procfs_self_callbacks, 0x0, FS_VIRTUAL_FLAGS | FS_NO_MOUNT_FLAGS);
    if (procfs_id == -EINVAL || proc_self_id == -EINVAL) {
        kerror("procfs register error (%d) (%d)", procfs_id, proc_self_id);
    }
}

void procfs_on_new_task(pcb_t task) {
    if (procfs_root == NULL)
        return;

    char name[MAX_PID_NAME_LEN];
    sprintf(name, "%d", task->pid);

    char *root_path = vfs_get_fullpath(procfs_root);
    char fname[strlen(root_path) + 1 + MAX_PID_NAME_LEN];
    sprintf(fname, "%s/%d", root_path, task->pid);
    vfs_node_t pro = vfs_open(fname);
    free(root_path);
    if (pro != NULL) {
        vfs_close(pro);
        return;
    }

    vfs_node_t node   = vfs_child_append(procfs_root, name, NULL);
    node->type        = file_dir;
    node->mode        = 0644;
    node->fsid        = procfs_id;
    task->procfs_node = node;

    vfs_node_t cmdline    = vfs_child_append(node, "cmdline", NULL);
    cmdline->type         = file_none;
    cmdline->mode         = 0700;
    cmdline->fsid         = procfs_id;
    proc_handle_t *handle = malloc(sizeof(proc_handle_t));
    cmdline->handle       = handle;
    handle->task          = task;
    sprintf(handle->name, "proc_cmdline");

    vfs_node_t mmaps        = vfs_child_append(node, "maps", NULL);
    mmaps->type             = file_none;
    mmaps->mode             = 0700;
    mmaps->fsid             = procfs_id;
    proc_handle_t *handle_m = malloc(sizeof(proc_handle_t));
    mmaps->handle           = handle_m;
    handle_m->task          = task;
    sprintf(handle_m->name, "proc_maps");

    vfs_node_t self_stat            = vfs_child_append(node, "stat", NULL);
    self_stat->type                 = file_none;
    self_stat->mode                 = 0700;
    proc_handle_t *self_stat_handle = malloc(sizeof(proc_handle_t));
    self_stat->handle               = self_stat_handle;
    self_stat_handle->task          = task;
    sprintf(self_stat_handle->name, "proc_stat");
}

void procfs_on_exit_task(pcb_t task) {
    if (procfs_root == NULL)
        return;
    spin_lock(procfs_oplock);

    char *root_path = vfs_get_fullpath(procfs_root);
    char name[strlen(root_path) + 1 + MAX_PID_NAME_LEN];
    sprintf(name, "%s/%d", root_path, task->pid);
    free(root_path);

    vfs_node_t node = vfs_open(name);
    if (node && node->parent) {
        list_delete(node->parent->child, node);
        vfs_free(node);
    }

    spin_unlock(procfs_oplock);
}
