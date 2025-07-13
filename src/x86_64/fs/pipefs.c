#define ALL_IMPLEMENTATION
#include "pipefs.h"
#include "kprint.h"
#include "list.h"

vfs_node_t pipefs_root;
int        pipefs_id = 0;

void wake_blocked_tasks(task_block_list_t *head) {
    task_block_list_t *current = head->next;
    head->next                 = NULL;

    while (current) {
        task_block_list_t *next = current->next;
        if (current->thread) { current->thread->status = START; }
        free(current);
        current = next;
    }
}

void pipefs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
}

size_t pipefs_read(void *file, void *addr, size_t offset, size_t size) {
    if (size > PIPE_BUFF) size = PIPE_BUFF;

    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return -EINVAL;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return -EINVAL;

    spin_lock(pipe->lock);

    uint32_t available = (pipe->write_ptr - pipe->read_ptr) % PIPE_BUFF;
    if (available == 0) {
        fd_file_handle *fd = container_of(spec->node, fd_file_handle, node);
        if (fd->flags & O_NONBLOCK) {
            spin_unlock(pipe->lock);
            return -EWOULDBLOCK;
        }

        if (pipe->write_fds == 0) {
            spin_unlock(pipe->lock);
            return 0;
        }
        close_interrupt;
        task_block_list_t *new_block = malloc(sizeof(task_block_list_t));
        new_block->thread            = get_current_task();
        new_block->next              = NULL;

        task_block_list_t *browse = &pipe->blocking_read;
        while (browse->next)
            browse = browse->next;
        browse->next = new_block;

        spin_unlock(pipe->lock);

        task_block(get_current_task(), WAIT, -1);
    }
    spin_unlock(pipe->lock);

    available = (pipe->write_ptr - pipe->read_ptr) % PIPE_BUFF;

    // 实际读取量
    uint32_t to_read = MIN(size, available);

    if (available == 0) {
        spin_unlock(pipe->lock);
        return 0;
    }

    // 分两种情况拷贝数据
    if (pipe->read_ptr + to_read <= PIPE_BUFF) {
        memcpy(addr, &pipe->buf[pipe->read_ptr], to_read);
    } else {
        uint32_t first_chunk = PIPE_BUFF - pipe->read_ptr;
        memcpy(addr, &pipe->buf[pipe->read_ptr], first_chunk);
        memcpy(addr + first_chunk, pipe->buf, to_read - first_chunk);
    }

    // 更新读指针
    pipe->read_ptr = (pipe->read_ptr + to_read) % PIPE_BUFF;

    wake_blocked_tasks(&pipe->blocking_write);

    spin_unlock(pipe->lock);

    return to_read;
}

size_t pipe_write_inner(void *file, const void *addr, size_t size) {
    if (size > PIPE_BUFF) size = PIPE_BUFF;

    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    spin_lock(pipe->lock);

    uint32_t free_space = PIPE_BUFF - ((pipe->write_ptr - pipe->read_ptr) % PIPE_BUFF);
    if (free_space < size) {
        if (pipe->read_fds == 0) {
            spin_unlock(pipe->lock);
            return -EPIPE;
        }
        task_block_list_t *new_block = malloc(sizeof(task_block_list_t));
        new_block->thread            = get_current_task();
        new_block->next              = NULL;

        task_block_list_t *browse = &pipe->blocking_write;

        while (browse->next)
            browse = browse->next;
        browse->next = new_block;

        spin_unlock(pipe->lock);

        task_block(get_current_task(), WAIT, -1);

        while (get_current_task()->status == WAIT) {
            open_interrupt;
            __asm__("pause");
        }
        close_interrupt;
    }

    if (pipe->write_ptr + size <= PIPE_BUFF) {
        memcpy(&pipe->buf[pipe->write_ptr], addr, size);
    } else {
        uint32_t first_chunk = PIPE_BUFF - pipe->write_ptr;
        memcpy(&pipe->buf[pipe->write_ptr], addr, first_chunk);
        memcpy(pipe->buf, addr + first_chunk, size - first_chunk);
    }

    pipe->write_ptr = (pipe->write_ptr + size) % PIPE_BUFF;

    wake_blocked_tasks(&pipe->blocking_read);

    spin_unlock(pipe->lock);

    return size;
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    int    ret       = 0;
    size_t chunks    = size / PIPE_BUFF;
    size_t remainder = size % PIPE_BUFF;
    if (chunks)
        for (size_t i = 0; i < chunks; i++) {
            int cycle = 0;
            while (cycle != PIPE_BUFF)
                cycle += pipe_write_inner(file, addr + i * PIPE_BUFF + cycle, PIPE_BUFF - cycle);
            ret += cycle;
        }

    if (remainder) {
        size_t cycle = 0;
        while (cycle != remainder)
            cycle += pipe_write_inner(file, addr + chunks * PIPE_BUFF + cycle, remainder - cycle);
        ret += cycle;
    }

    return ret;
}

int pipefs_ioctl(void *file, ssize_t cmd, ssize_t arg) {
    switch (cmd) {
    default: return -ENOSYS;
    }
}

bool pipefs_close(void *current) {
    pipe_specific_t *spec = (pipe_specific_t *)current;
    pipe_info_t     *pipe = spec->info;

    spin_lock(pipe->lock);
    if (spec->write) {
        pipe->write_fds--;
    } else {
        pipe->read_fds--;
    }

    if (!pipe->write_fds) wake_blocked_tasks(&pipe->blocking_read);
    if (!pipe->read_fds) wake_blocked_tasks(&pipe->blocking_write);

    if (pipe->write_fds == 0 && pipe->read_fds == 0) { free(pipe); }
    spin_unlock(pipe->lock);

    free(spec);

    list_delete(pipefs_root->child, spec->node);

    return true;
}

int pipefs_poll(void *file, size_t events) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    int out = 0;

    spin_lock(pipe->lock);
    if (events & EPOLLIN) {
        if (!pipe->write_fds)
            out |= EPOLLHUP;
        else if (pipe->write_ptr != pipe->read_ptr)
            out |= EPOLLIN;
    }

    if (events & EPOLLOUT) {
        if (!pipe->read_fds)
            out |= EPOLLERR;
        else if (pipe->assigned < PIPE_BUFF)
            out |= EPOLLOUT;
    }
    spin_unlock(pipe->lock);
    return out;
}

int pipefs_mount(const char *handle, vfs_node_t node) {
    if ((uint64_t)handle != PIEFS_REGISTER_ID) return VFS_STATUS_FAILED;
    node->fsid  = pipefs_id;
    pipefs_root = node;
    return VFS_STATUS_SUCCESS;
}

static struct vfs_callback pipefs_callbacks = {
    .mount   = pipefs_mount,
    .unmount = (vfs_unmount_t)empty,
    .open    = (vfs_open_t)pipefs_open,
    .close   = (vfs_close_t)pipefs_close,
    .read    = pipefs_read,
    .write   = pipefs_write,
    .mkdir   = (vfs_mk_t)empty,
    .mkfile  = (vfs_mk_t)empty,
    .delete  = (vfs_del_t)empty,
    .rename  = (vfs_rename_t)empty,
    .map     = (vfs_mapfile_t)empty,
    .stat    = (vfs_stat_t)empty,
    .ioctl   = (vfs_ioctl_t)pipefs_ioctl,
    .poll    = pipefs_poll,
};

void pipefs_setup() {
    pipefs_id = vfs_regist("pipefs", &pipefs_callbacks);
    vfs_mkdir("/pipe");
    vfs_node_t dev = vfs_open("/pipe");
    if (dev == NULL) {
        kerror("'pipe' handle is null.");
        return;
    }
    if (vfs_mount((const char *)PIEFS_REGISTER_ID, dev) == VFS_STATUS_FAILED) {
        kerror("Cannot mount pipe file system.");
    }
}
