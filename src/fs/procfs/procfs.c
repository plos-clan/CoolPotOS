#define ALL_IMPLEMENTATION
#include "fs/procfs.h"
#include "bootarg.h"
#include "errno.h"
#include "fs/fds.h"
#include "intctl.h"
#include "mem/vma.h"
#include "task/smp.h"
#include "term/klog.h"

static int procfs_id     = 0;
static int proc_self_id  = 0;
vfs_node_t procfs_root   = NULL;
spin_t     procfs_oplock = SPIN_INIT;

extern const char filesystems_content[];

const char *get_vma_permissions(vma_t *vma) {
    static char perms[5];

    perms[0] = (vma->vm_flags & VMA_READ) ? 'r' : '-';
    perms[1] = (vma->vm_flags & VMA_WRITE) ? 'w' : '-';
    perms[2] = (vma->vm_flags & VMA_EXEC) ? 'x' : '-';
    perms[3] = (vma->vm_flags & VMA_SHARED) ? 's' : 'p';
    perms[4] = '\0';

    return perms;
}

char *proc_gen_maps_file(pcb_t task, size_t *content_len) {
    vma_t *vma = task->vma_manager.vma_list;

    size_t offset  = 0;
    size_t ctn_len = PAGE_SIZE;
    char  *buf     = malloc(ctn_len);

    while (vma) {
        vfs_node_t node = NULL;
        if (vma->vm_fd != -1) {
            fd_t *fd_handle = get_fd(get_current_task()->process->fdts, vma->vm_fd);
            node            = fd_handle->node;
        }

        int len = sprintf(buf + offset, "%012lx-%012lx %s %08lx %02x:%02x %lu", vma->vm_start,
                          vma->vm_end, get_vma_permissions(vma), (unsigned long)vma->vm_offset, 0,
                          0, node ? node->inode : 0);

        if (offset + len > ctn_len) {
            ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            buf     = realloc(buf, ctn_len);
        }
        offset += len;

        const char *pathname = vma->vm_name;
        if (pathname && strlen(pathname) > 0) {
            len = sprintf(buf + offset, "%*s%s", 15, "", pathname);
            if (offset + len > ctn_len) {
                ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                buf     = realloc(buf, ctn_len);
            }
            offset += len;
        }

        len = sprintf(buf + offset, "\n");
        if (offset + len > ctn_len) {
            ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            buf     = realloc(buf, ctn_len);
        }
        offset += len;

        vma = vma->vm_next;
    }

    *content_len = offset;

    return buf;
}

char *proc_gen_stat_file(pcb_t task, size_t *content_len) {
    char *buffer = malloc(PAGE_SIZE * 4);
    int   len    = sprintf(buffer,
                           "%d (%s) %c %d %d %d %d %d %u %d %d %d %d %d %d %d %d %d %d "
                                "%ld %d %d %lu %d %d %d %d %d %d %d %d %d %d %d %d %d "
                                "%d %d %d %u %u %d %d %d %d %d %d %d %d %d %d %d\n",
                           task->pid,  // pid
                           task->name, // name
                      task->status == T_RUNNING  ? 'R'
                           : task->status == T_ZOMBIE ? 'Z'
                           : task->status == T_FUTEX  ? 'S'
                                                      : 'T',              // state
                           task->parent->pid,                            // ppid
                           0,                                            // pgrp
                           task->uid,                                    // session
                           0,                                            // tty_nr
                           0,                                            // tpgid
                           0,                                            // flags
                           0,                                            // minflt
                           0,                                            // cminflt
                           0,                                            // majflt
                           0,                                            // cmajflt
                           0,                                            // utime
                           0,                                            // stime
                           0,                                            // cutime
                           0,                                            // cstime
                      task->pid == 0 ? SCHED_IDLE : SCHED_DEADLINE, // priority
                           0,                                            // nicec
                           task->child_threads->size,                    // num_threads
                           0,                                            // itrealvalue
                           0,                                            // starttime
                           task->vma_manager.vm_total,                   // vsize
                           0,                                            // rss
                           0,                                            // rsslim
                           0,                                            // startcode
                           0,                                            // endcode
                           0,                                            // startstack
                           0,                                            // kstkesp
                           0,                                            // ksteip
                           0,                                            // signal
                           0,                                            // blocked
                           0,                                            // sigignore
                           0,                                            // sigcatch
                           0,                                            // wchan
                           0,                                            // nswap
                           0,                                            // cnswap
                           0,                                            // exit_signal
                           0,                                            // processor
                           0,                                            // rt_priority
                           0,                                            // policy
                           0,                                            // delayacct_blkio_ticks
                           0,                                            // guest_time
                           0,                                            // cguest_time
                           0,                                            // start_data
                           0,                                            // end_data
                           0,                                            // start_brk
                           0,                                            // arg_start
                           0,                                            // arg_end
                           0,                                            // env_start
                           0,                                            // env_end
                           0                                             // exit_code
         );

    *content_len = len;

    return buffer;
}

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
    if (!handle) { return -1; }
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }

    if (!strcmp(handle->name, "proc_cmdline")) {
        char  *cmdline = task->cmdline ? task->cmdline : "no_cmdline";
        size_t len     = strlen(cmdline);
        char  *contect = strdup(cmdline);
        return procfs_node_read(len, offset, size, addr, contect);
    } else if (!strcmp(handle->name, "proc_maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(handle->task, &content_len);
        if (offset >= content_len) {
            free(content);
            return 0;
        }
        content_len    = MIN(content_len, offset + size);
        size_t to_copy = MIN(content_len, size);
        memcpy(addr, content + offset, to_copy);
        free(content);
        ((char *)addr)[to_copy] = '\0';
        return to_copy;
    } else if (!strcmp(handle->name, "proc_stat")) {
        size_t content_len = 0;
        char  *content     = proc_gen_stat_file(task, &content_len);
        if (offset >= content_len) {
            free(content);
            return 0;
        }
        content_len    = MIN(content_len, offset + size);
        size_t to_copy = MIN(content_len, size);
        memcpy(addr, content + offset, to_copy);
        free(content);
        ((char *)addr)[to_copy] = '\0';
        return to_copy;
    }

    return procfs_read_dispatch(handle, addr, offset, size);
}

vfs_node_t procfs_dup(vfs_node_t src) {
    return src;
}

errno_t procfs_stat(void *file, vfs_node_t node) {
    if (file == NULL) return EOK;
    proc_handle_t *handle = file;
    if (!strcmp(handle->name, "proc_maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(handle->task, &content_len);
        free(content);
        node->size = content_len;
    } else if (!strcmp(handle->name, "proc_stat")) {
        size_t content_len = 0;
        char  *content     = proc_gen_stat_file(handle->task, &content_len);
        node->size         = content_len;
        free(content);
    }
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
    procfs_self_handle_t *handle  = current;
    handle->self->type           |= file_delete;
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
    if (!(file->type & file_symlink)) return 0;
    if (offset >= strlen(file->linkto->name)) return 0;
    logkf("procfs: readlink offset:%llu size:%llu", offset, size);
    char   *ptr = file->linkto->name + offset;
    ssize_t len = strlen(ptr);
    len         = MIN(len, (ssize_t)size);
    memcpy(addr, ptr, len);
    return len;
}

errno_t procfs_self_stat(void *file, vfs_node_t node) {
    procfs_self_handle_t *handle  = file;
    node->type                   |= file_symlink;
    node->linkto                  = get_current_task()->process->procfs_node;
    node->size                    = strlen(node->linkto->name);
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
    procfs_id    = vfs_regist("proc", &procfs_callbacks, 0x9fa0);
    proc_self_id = vfs_regist("proc_self", &procfs_self_callbacks, 0x0);
    if (procfs_id == -EINVAL || proc_self_id == -EINVAL) {
        kerror("procfs register error (%d) (%d)", procfs_id, proc_self_id);
    }
}

void procfs_on_new_task(pcb_t task) {
    if (procfs_root == NULL) return;

    char name[MAX_PID_NAME_LEN];
    sprintf(name, "%d", task->pid);

    char *root_path = vfs_get_fullpath(procfs_root);
    char  fname[strlen(root_path) + 1 + MAX_PID_NAME_LEN];
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
    if (procfs_root == NULL) return;
    spin_lock(procfs_oplock);

    char *root_path = vfs_get_fullpath(procfs_root);
    char  name[strlen(root_path) + 1 + MAX_PID_NAME_LEN];
    sprintf(name, "%s/%d", root_path, task->pid);
    free(root_path);

    vfs_node_t node = vfs_open(name);
    if (node && node->parent) {
        list_delete(node->parent->child, node);
        vfs_free(node);
    }

    spin_unlock(procfs_oplock);
}
