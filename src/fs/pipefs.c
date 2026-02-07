#define ALL_IMPLEMENTATION
#include "fs/pipefs.h"
#include "errno.h"
#include "krlibc.h"
#include "task/scheduler.h"
#include "term/klog.h"

vfs_node_t pipefs_root = NULL;
int        pipefs_id   = 0;
int        pipefd_id   = 0;

void pipefs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_pipe;
}

size_t pipefs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    if (size > PIPE_BUFF) size = PIPE_BUFF;

    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return (size_t)-1;
    if (spec->write) return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return (size_t)-1;

    spin_lock(pipe->lock);
    spec->active++;
    pipe->active++;
    spin_unlock(pipe->lock);

    size_t ret = (size_t)-1;
    for (;;) {
        spin_lock(pipe->lock);

        // 检查是否有数据可读
        if (pipe->ptr > 0) {
            // 实际读取量
            uint32_t to_read = MIN(size, pipe->ptr);

            memcpy(addr, pipe->buf, to_read);
            memmove(pipe->buf, pipe->buf + to_read, pipe->ptr - to_read);

            pipe->ptr      -= to_read;
            pipe->assigned  = (int)pipe->ptr;
            if (spec->node) spec->node->size = pipe->ptr;

            spin_unlock(pipe->lock);
            ret = to_read;
            goto out;
        }

        // 没有数据，检查写端是否已关闭
        if (pipe->write_fds == 0) {
            spin_unlock(pipe->lock);
            ret = 0;  // EOF
            goto out;
        }

        // 没有数据且写端还开着，等待
        spin_unlock(pipe->lock);
        scheduler_yield();
    }

out:
    spin_lock(pipe->lock);
    spec->active--;
    pipe->active--;
    bool free_spec = spec->free_pending && spec->active == 0;
    bool free_pipe =
        pipe->free_pending && pipe->active == 0 && pipe->read_fds == 0 && pipe->write_fds == 0;
    spin_unlock(pipe->lock);

    vfs_node_t node = spec->node;
    if (free_spec) {
        if (node) node->handle = NULL;
        free(spec);
    }
    if (free_pipe) {
        free(pipe->buf);
        free(pipe);
    }
    if (free_spec && node && node->refcount == 0) {
        if (node->parent) list_delete(node->parent->child, node);
        vfs_free(node);
    }
    return ret;
}

size_t pipe_write_inner(void *file, const void *addr, size_t size) {
    pipe_specific_t *spec = file;
    if (!spec || !spec->write) return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return (size_t)-1;
    if (pipe->read_fds == 0) { return (size_t)-1; }

    for (;;) {
        // 等待有可用空间（至少1字节）
        while (pipe->ptr >= PIPE_BUFF) {
            if (pipe->read_fds == 0) { return (size_t)-1; }
            scheduler_yield();
        }

        spin_lock(pipe->lock);
        size_t available = PIPE_BUFF - pipe->ptr;
        if (available == 0) {
            spin_unlock(pipe->lock);
            continue;
        }
        // 写入尽可能多的数据（部分写入）
        size_t to_write = MIN(size, available);
        memcpy(&pipe->buf[pipe->ptr], addr, to_write);
        pipe->ptr      += to_write;
        pipe->assigned  = (int)pipe->ptr;
        if (spec->node) spec->node->size = pipe->ptr;
        spin_unlock(pipe->lock);
        return to_write;
    }
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    pipe_specific_t *spec = file;
    if (!spec || !spec->write) return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return (size_t)-1;
    const uint8_t *src = (const uint8_t *)addr;
    size_t         ret = 0;
    size_t         chunks = size / PIPE_BUFF;
    size_t         remainder = size % PIPE_BUFF;

    spin_lock(pipe->lock);
    spec->active++;
    pipe->active++;
    spin_unlock(pipe->lock);

    if (chunks)
        for (size_t i = 0; i < chunks; i++) {
            size_t cycle = 0;
            while (cycle != PIPE_BUFF) {
                const size_t ret1 =
                    pipe_write_inner(file, src + i * PIPE_BUFF + cycle, PIPE_BUFF - cycle);
                if (ret1 == (size_t)-1) {
                    ret = (size_t)-1;
                    goto out;
                }
                cycle += ret1;
            }
            ret += cycle;
        }

    if (remainder) {
        size_t cycle = 0;
        while (cycle != remainder) {
            const size_t ret0 =
                pipe_write_inner(file, src + chunks * PIPE_BUFF + cycle, remainder - cycle);
            if (ret0 == (size_t)-1) {
                ret = (size_t)-1;
                goto out;
            }
            cycle += ret0;
        }
        ret += cycle;
    }

out:
    spin_lock(pipe->lock);
    spec->active--;
    pipe->active--;
    bool free_spec = spec->free_pending && spec->active == 0;
    bool free_pipe =
        pipe->free_pending && pipe->active == 0 && pipe->read_fds == 0 && pipe->write_fds == 0;
    spin_unlock(pipe->lock);

    vfs_node_t node = spec->node;
    if (free_spec) {
        if (node) node->handle = NULL;
        free(spec);
    }
    if (free_pipe) {
        free(pipe->buf);
        free(pipe);
    }
    if (free_spec && node && node->refcount == 0) {
        if (node->parent) list_delete(node->parent->child, node);
        vfs_free(node);
    }
    return ret;
}

int pipefs_ioctl(void *file, ssize_t cmd, ssize_t arg) {
    (void)file;
    (void)arg;
    switch (cmd) {
    default: return -ENOSYS;
    }
}

bool pipefs_close(void *current) {
    pipe_specific_t *spec = (pipe_specific_t *)current;
    if (!spec) return true;
    pipe_info_t *pipe = spec->info;
    if (!pipe) {
        if (spec->node) spec->node->handle = NULL;
        free(spec);
        return true;
    }

    bool free_spec = false;
    bool free_pipe = false;

    spin_lock(pipe->lock);
    if (spec->write) {
        if (pipe->write_fds > 0) pipe->write_fds--;
        if (pipe->write_fds == 0) {
            if (spec->active == 0) {
                free_spec = true;
            } else {
                spec->free_pending = true;
            }
        }
    } else {
        if (pipe->read_fds > 0) pipe->read_fds--;
        if (pipe->read_fds == 0) {
            if (spec->active == 0) {
                free_spec = true;
            } else {
                spec->free_pending = true;
            }
        }
    }

    if (pipe->write_fds == 0 && pipe->read_fds == 0) {
        if (pipe->active == 0) {
            free_pipe = true;
        } else {
            pipe->free_pending = true;
        }
    }
    spin_unlock(pipe->lock);

    if (free_spec) {
        if (spec->node) spec->node->handle = NULL;
        free(spec);
    }

    if (free_pipe) {
        free(pipe->buf);
        free(pipe);
    }

    return true;
}

int pipefs_poll(void *file, size_t events) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return 0;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return 0;

    int out = 0;

    spin_lock(pipe->lock);
    if (events & EPOLLIN) {
        if (!pipe->write_fds) out |= EPOLLHUP;
        if (pipe->ptr > 0) out |= EPOLLIN;
    }

    if (events & EPOLLOUT) {
        if (!pipe->read_fds) out |= EPOLLHUP;
        if (pipe->ptr < PIPE_BUFF) out |= EPOLLOUT;
    }
    spin_unlock(pipe->lock);
    return out;
}

int pipefs_mount(const char *handle, vfs_node_t node) {
    if (pipefs_root != NULL) return -EBUSY;
    node->fsid   = pipefs_id;
    pipefs_root  = node;
    node->handle = calloc(1, sizeof(pipe_specific_t));
    return EOK;
}

errno_t pipefs_stat(void *file, vfs_node_t node) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return EOK;
    pipe_info_t     *pipe = spec->info;
    if (pipe == NULL) return EOK;
    node->size = pipe->ptr;
    return EOK;
}

errno_t pipefs_free(void *handle) {
    (void)handle;
    return EOK;
}

static struct vfs_callback pipefs_callbacks = {
    .mount    = pipefs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)pipefs_open,
    .close    = pipefs_close,
    .read     = pipefs_read,
    .write    = pipefs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = pipefs_stat,
    .ioctl    = (vfs_ioctl_t)pipefs_ioctl,
    .poll     = pipefs_poll,
    .dup      = (vfs_dup_t)dummy,
    .free     = pipefs_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void pipefs_regist() {
    pipefs_id = vfs_regist("pipefs", &pipefs_callbacks, 0x50495045, FS_VIRTUAL_FLAGS);
    if (pipefs_id == -EINVAL) { kerror("pipefs regist error."); }
}
