#define ALL_IMPLEMENTATION
#include "procfs.h"
#include "boot.h"
#include "errno.h"
#include "klog.h"
#include "kprint.h"
#include "pcb.h"
#include "sprintf.h"
#include "vfs.h"

static int procfs_id     = 0;
vfs_node_t procfs_root   = NULL;
spin_t     procfs_oplock = SPIN_INIT;

const char filesystems_content[] = //"nodev\tsysfs\n"
    "nodev\ttmpfs\n"
    "nodev\tproc\n"
    "nodev\tmodfs\n"
    "     \text4\n"
    "     \text3\n"
    "     \text2\n";

static int dummy() {
    return 0;
}

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
            fd_file_handle *fd_handle =
                queue_get(get_current_task()->parent_group->file_open, vma->vm_fd);
            node = fd_handle->node;
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

errno_t procfs_mount(const char *src, vfs_node_t node) {
    if (src != (void *)PROC_REGISTER_ID) return VFS_STATUS_FAILED;
    procfs_root = node;

    procfs_root->fsid = procfs_id;

    vfs_node_t procfs_self = vfs_node_alloc(procfs_root, "self");
    procfs_self->type      = file_dir;
    procfs_self->mode      = 0644;

    vfs_node_t self_environ            = vfs_node_alloc(procfs_self, "environ");
    self_environ->type                 = file_none;
    self_environ->mode                 = 0700;
    proc_handle_t *self_environ_handle = malloc(sizeof(proc_handle_t));
    self_environ->handle               = self_environ_handle;
    self_environ_handle->task          = NULL;
    sprintf(self_environ_handle->name, "self/environ");

    vfs_node_t self_maps            = vfs_node_alloc(procfs_self, "maps");
    self_maps->type                 = file_none;
    self_maps->mode                 = 0700;
    proc_handle_t *self_maps_handle = malloc(sizeof(proc_handle_t));
    self_maps->handle               = self_maps_handle;
    self_maps_handle->task          = NULL;
    sprintf(self_maps_handle->name, "self/maps");

    vfs_node_t self_cmdline            = vfs_node_alloc(procfs_self, "cmdline");
    self_cmdline->type                 = file_none;
    self_cmdline->mode                 = 0700;
    proc_handle_t *self_cmdline_handle = malloc(sizeof(proc_handle_t));
    self_cmdline->handle               = self_cmdline_handle;
    self_cmdline_handle->task          = NULL;
    sprintf(self_cmdline_handle->name, "self/cmdline");

    vfs_node_t cmdline    = vfs_node_alloc(procfs_root, "cmdline");
    cmdline->type         = file_none;
    cmdline->mode         = 0700;
    proc_handle_t *handle = malloc(sizeof(proc_handle_t));
    cmdline->handle       = handle;
    handle->task          = NULL;
    sprintf(handle->name, "cmdline");

    vfs_node_t filesystems            = vfs_node_alloc(procfs_root, "filesystems");
    filesystems->type                 = file_none;
    filesystems->mode                 = 0700;
    proc_handle_t *filesystems_handle = malloc(sizeof(proc_handle_t));
    filesystems->handle               = filesystems_handle;
    filesystems_handle->task          = NULL;
    sprintf(filesystems_handle->name, "filesystems");

    return VFS_STATUS_SUCCESS;
}

void procfs_open(void *parent, const char *name, vfs_node_t node) {
    logkf("procfs: open file(%s) node_name: %s\n", name, node->name);
}

void procfs_close(void *current) {}

size_t procfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    return 0;
}

size_t procfs_write(void *file, const void *addr, size_t offset, size_t size) {
    return size;
}

size_t procfs_read(void *file, void *addr, size_t offset, size_t size) {
    proc_handle_t *handle = (proc_handle_t *)file;
    if (!handle) { return VFS_STATUS_FAILED; }
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->parent_group;
    } else {
        task = handle->task;
    }

    if (!strcmp(handle->name, "filesystems")) {
        size_t fs_size = strlen(filesystems_content);
        if (offset < fs_size) {
            if (size > fs_size) size = fs_size;
            memcpy(addr, filesystems_content + offset, size);
            return size;
        } else
            return 0;
    } else if (!strcmp(handle->name, "cmdline")) {
        ssize_t len = strlen(get_kernel_cmdline());
        if (len == 0) return 0;
        len = (len + 1) > size ? size : len + 1;
        memcpy(addr, get_kernel_cmdline(), len);
        return len;
    } else if (!strcmp(handle->name, "self/maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(get_current_task()->parent_group, &content_len);
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
    } else if (!strcmp(handle->name, "self/cmdline")) {
        ssize_t len = strlen(get_current_task()->parent_group->cmdline);
        if (len == 0) return 0;
        len = (len + 1) > size ? size : len + 1;
        memcpy(addr, get_current_task()->parent_group->cmdline, len);
        return len;
    } else if (!strcmp(handle->name, "proc_cmdline")) {
        ssize_t len = strlen(task->cmdline);
        if (len == 0) return 0;
        len = (len + 1) > size ? size : len + 1;
        memcpy(addr, task->cmdline, len);
        return len;
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
    } else if (!strcmp(handle->name, "dri_name")) {
        char name[] = "cpkernel_drm";
        int  len    = strlen(name);
        memcpy(addr, name, len);
        return len;
    }
}

vfs_node_t procfs_dup(vfs_node_t src) {
    return src;
}

errno_t procfs_stat(void *file, vfs_node_t node) {
    if (file == NULL) return EOK;
    proc_handle_t *handle = file;
    if (!strcmp(handle->name, "filesystems"))
        node->size = strlen(filesystems_content);
    else if (!strcmp(handle->name, "cmdline"))
        node->size = strlen(get_kernel_cmdline());
    else if (!strcmp(handle->name, "self/maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(get_current_task()->parent_group, &content_len);
        free(content);
        node->size = content_len;
    } else if (!strcmp(handle->name, "proc_cmdline"))
        node->size = strlen(handle->task->cmdline);
    else if (!strcmp(handle->name, "self/cmdline"))
        node->size = strlen(get_current_task()->parent_group->cmdline);
    else if (!strcmp(handle->name, "proc_maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(handle->task, &content_len);
        free(content);
        node->size = content_len;
    } else if (!strcmp(handle->name, "dri_name"))
        node->size = strlen("cpkernel_drm");
}

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
};

void procfs_setup() {
    procfs_id = vfs_regist("proc", &procfs_callbacks, PROC_REGISTER_ID);
    if (procfs_id == VFS_STATUS_FAILED) { kerror("procfs register error"); }
}

void procfs_on_new_task(pcb_t task) {
    if (procfs_root == NULL) return;
    spin_lock(procfs_oplock);

    char name[MAX_PID_NAME_LEN];
    sprintf(name, "%d", task->pid);

    char fname[6 + MAX_PID_NAME_LEN];
    sprintf(fname, "/proc/%d", task->pid);
    vfs_node_t pro = vfs_open(fname);
    if (pro != NULL) {
        vfs_close(pro);
        spin_unlock(procfs_oplock);
        return;
    }

    vfs_node_t node = vfs_child_append(procfs_root, name, NULL);
    node->type      = file_dir;
    node->mode      = 0644;
    node->fsid      = procfs_id;

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

    spin_unlock(procfs_oplock);
}

void procfs_on_exit_task(pcb_t task) {
    if (procfs_root == NULL) return;
    spin_lock(procfs_oplock);

    char name[6 + MAX_PID_NAME_LEN];
    sprintf(name, "/proc/%d", task->pid);

    vfs_node_t node = vfs_open(name);
    if (node && node->parent) {
        list_delete(node->parent->child, node);
        vfs_free(node);
    }

    spin_unlock(procfs_oplock);
}

extern lock_queue *pgb_queue;

void procfs_update_task_list() {
    if (procfs_root == NULL) return;
    do {
        vfs_node_t proc_node = NULL;
        list_foreach(procfs_root->child, node) {
            vfs_node_t pnode = node->data;
            if (pnode->handle != NULL) {
                proc_handle_t *handle = pnode->handle;
                if (handle->task == NULL) continue;
                proc_node = pnode;
                break;
            }
        }
        if (proc_node == NULL) break;
        proc_handle_t *handle = proc_node->handle;
        if (handle->task->status == DEATH) {
            if (proc_node && proc_node->parent) {
                list_delete(proc_node->parent->child, proc_node);
                vfs_free(proc_node);
            }
        }
    } while (true);
    queue_foreach(pgb_queue, node) {
        pcb_t process = node->data;
        procfs_on_new_task(process);
    }
}
