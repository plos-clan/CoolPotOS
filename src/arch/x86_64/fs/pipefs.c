#define ALL_IMPLEMENTATION
#include "pipefs.h"
#include "kprint.h"
#include "list.h"

vfs_node_t pipefs_root = NULL;
int        pipefs_id   = 0;
int        pipefd_id   = 0;

void pipefs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_pipe;
}

size_t pipefs_read(void *file, void *addr, size_t offset, size_t size) {
    if (size > PIPE_BUFF) size = PIPE_BUFF;

    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return -EINVAL;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return -EINVAL;

    while (pipe->ptr == 0) {
        if (pipe->write_fds == 0) { return 0; }

        scheduler_yield();
    }

    // 实际读取量
    uint32_t to_read = MIN(size, pipe->ptr);

    if (to_read == 0) { return 0; }

    spin_lock(pipe->lock);

    memcpy(addr, pipe->buf, to_read);
    memmove(pipe->buf, &pipe->buf[pipe->ptr], pipe->ptr - to_read);

    pipe->ptr -= to_read;

    spin_unlock(pipe->lock);

    return to_read;
}

size_t pipe_write_inner(void *file, const void *addr, size_t size) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    while ((PIPE_BUFF - pipe->ptr) < size) {
        if (pipe->read_fds == 0) { return -EPIPE; }
        scheduler_yield();
    }

    spin_lock(pipe->lock);
    memcpy(&pipe->buf[pipe->ptr], addr, size);
    pipe->ptr += size;
    spin_unlock(pipe->lock);

    return size;
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    size_t ret       = 0;
    size_t chunks    = size / PIPE_BUFF;
    size_t remainder = size % PIPE_BUFF;
    if (chunks)
        for (size_t i = 0; i < chunks; i++) {
            size_t cycle = 0;
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

    list_delete(pipefs_root->child, spec->node);
    if ((spec->write && pipe->write_fds == 0) || pipe->read_fds == 0) free(spec);

    if (pipe->write_fds == 0 && pipe->read_fds == 0) {
        spin_unlock(pipe->lock);
        free(pipe->buf);
        free(pipe);
        return true;
    }

    spin_unlock(pipe->lock);

    return true;
}

int pipefs_poll(void *file, size_t events) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    int out = 0;

    spin_lock(pipe->lock);
    if (events & EPOLLIN) {
        if (!pipe->write_fds) out |= EPOLLHUP;
        if (pipe->assigned > 0) out |= EPOLLIN;
    }

    if (events & EPOLLOUT) {
        if (!pipe->read_fds) out |= EPOLLHUP;
        if (pipe->assigned < PIPE_BUFF) out |= EPOLLOUT;
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

static int dummy() {
    return -ENOSYS;
}

errno_t pipefs_stat(void *file, vfs_node_t node) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;
    if(pipe == NULL) return EOK;
    node->size = pipe->ptr;
    return EOK;
}

static struct vfs_callback pipefs_callbacks = {
    .mount    = pipefs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = (vfs_open_t)pipefs_open,
    .close    = (vfs_close_t)pipefs_close,
    .read     = pipefs_read,
    .write    = pipefs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)empty,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)empty,
    .rename   = (vfs_rename_t)empty,
    .map      = (vfs_mapfile_t)empty,
    .stat     = pipefs_stat,
    .ioctl    = (vfs_ioctl_t)pipefs_ioctl,
    .poll     = pipefs_poll,
    .dup      = (vfs_dup_t)empty,
};

void pipefs_setup() {
    pipefs_id = vfs_regist("pipefs", &pipefs_callbacks, PIEFS_REGISTER_ID, 0x50495045);
}
