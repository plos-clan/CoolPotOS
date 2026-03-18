#define ALL_IMPLEMENTATION
#include "fs/pipefs.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "krlibc.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "term/klog.h"

vfs_node_t pipefs_root = NULL;
int pipefs_id          = 0;
int pipefd_id          = 0;

static void pipefs_update_nodes(pipe_info_t *pipe) {
    if (pipe->read_node) {
        pipe->read_node->size = pipe->ptr;
    }
    if (pipe->write_node) {
        pipe->write_node->size = pipe->ptr;
    }
}

static void pipefs_release_node(vfs_node_t node) {
    if (node == NULL) {
        return;
    }
    if (node->parent) {
        list_delete(node->parent->child, node);
    }
    node->child = list_free(node->child);
    free(node->name);
    if (node->linkto_path) {
        free(node->linkto_path);
    }
    free(node);
}

static void pipefs_leave(pipe_specific_t *spec) {
    if (spec == NULL || spec->info == NULL) {
        return;
    }

    pipe_info_t *pipe = spec->info;
    bool free_spec    = false;
    bool free_pipe    = false;

    spin_lock(pipe->lock);
    spec->active--;
    pipe->active--;
    free_spec = spec->free_pending && spec->active == 0;
    free_pipe =
        pipe->free_pending && pipe->active == 0 && pipe->read_fds == 0 && pipe->write_fds == 0;
    spin_unlock(pipe->lock);

    vfs_node_t node = spec->node;
    if (free_spec) {
        if (node) {
            node->handle = NULL;
        }
        free(spec);
    }
    if (free_pipe) {
        free(pipe->buf);
        free(pipe);
    }
    if (free_spec && node && node->refcount == 0) {
        pipefs_release_node(node);
    }
}

void pipefs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_pipe;
}

size_t pipefs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    if (size > PIPE_BUFF)
        size = PIPE_BUFF;
    if (size == 0) {
        return 0;
    }

    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec)
        return (size_t)-1;
    if (spec->write)
        return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe)
        return (size_t)-1;

    spin_lock(pipe->lock);
    spec->active++;
    pipe->active++;
    spin_unlock(pipe->lock);

    size_t ret = (size_t)-1;
    for (;;) {
        spin_lock(pipe->lock);
        if (pipe->ptr > 0) {
            const uint32_t to_read = MIN(size, pipe->ptr);

            memcpy(addr, pipe->buf, to_read);
            memmove(pipe->buf, pipe->buf + to_read, pipe->ptr - to_read);

            pipe->ptr -= to_read;
            pipe->assigned = (int)pipe->ptr;
            pipefs_update_nodes(pipe);
            spin_unlock(pipe->lock);
            if (pipe->write_node) {
                vfs_poll_notify(pipe->write_node, EPOLLOUT);
            }
            ret = to_read;
            goto out;
        }

        if (pipe->write_fds == 0) {
            spin_unlock(pipe->lock);
            ret = 0;
            goto out;
        }

        spin_unlock(pipe->lock);
        scheduler_yield();
    }

out:
    pipefs_leave(spec);
    return ret;
}

static size_t pipe_write_inner(pipe_specific_t *spec, const void *addr, size_t size) {
    if (!spec || !spec->write)
        return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe)
        return (size_t)-1;

    for (;;) {
        spin_lock(pipe->lock);
        if (pipe->read_fds == 0) {
            spin_unlock(pipe->lock);
            return (size_t)-1;
        }

        if ((PIPE_BUFF - pipe->ptr) >= size) {
            memcpy(&pipe->buf[pipe->ptr], addr, size);
            pipe->ptr += size;
            pipe->assigned = (int)pipe->ptr;
            pipefs_update_nodes(pipe);
            spin_unlock(pipe->lock);
            if (pipe->read_node) {
                vfs_poll_notify(pipe->read_node, EPOLLIN);
            }
            return size;
        }
        spin_unlock(pipe->lock);

        scheduler_yield();
    }
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    pipe_specific_t *spec = file;
    if (!spec || !spec->write)
        return (size_t)-1;
    pipe_info_t *pipe = spec->info;
    if (!pipe)
        return (size_t)-1;
    const uint8_t *src = (const uint8_t *)addr;
    size_t ret         = 0;
    const size_t chunks    = size / PIPE_BUFF;
    const size_t remainder = size % PIPE_BUFF;

    spin_lock(pipe->lock);
    spec->active++;
    pipe->active++;
    spin_unlock(pipe->lock);

    if (chunks)
        for (size_t i = 0; i < chunks; i++) {
            size_t cycle = 0;
            while (cycle != PIPE_BUFF) {
                const size_t ret1 = pipe_write_inner(spec, src + i * PIPE_BUFF + cycle,
                                                     PIPE_BUFF - cycle);
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
                pipe_write_inner(spec, src + chunks * PIPE_BUFF + cycle, remainder - cycle);
            if (ret0 == (size_t)-1) {
                ret = (size_t)-1;
                goto out;
            }
            cycle += ret0;
        }
        ret += cycle;
    }

out:
    pipefs_leave(spec);
    return ret;
}

int pipefs_ioctl(void *file, ssize_t cmd, ssize_t arg) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec || !spec->info || arg == 0) {
        return -EINVAL;
    }
    switch (cmd) {
    case FIONREAD:
        *(int *)arg = (int)spec->info->ptr;
        return EOK;
    default:
        return -ENOTTY;
    }
}

bool pipefs_close(void *current) {
    pipe_specific_t *spec = (pipe_specific_t *)current;
    if (!spec)
        return true;
    pipe_info_t *pipe = spec->info;
    if (!pipe) {
        if (spec->node)
            spec->node->handle = NULL;
        free(spec);
        return true;
    }

    bool free_spec = false;
    bool free_pipe = false;
    bool notify_read_hup = false;
    bool notify_write_hup = false;
    vfs_node_t read_node  = NULL;
    vfs_node_t write_node = NULL;

    spin_lock(pipe->lock);
    read_node  = pipe->read_node;
    write_node = pipe->write_node;

    if (spec->write) {
        if (pipe->write_fds > 0)
            pipe->write_fds--;
        if (pipe->write_fds == 0) {
            pipe->write_node = NULL;
            notify_read_hup = true;
            if (spec->active == 0) {
                free_spec = true;
            } else {
                spec->free_pending = true;
            }
        }
    } else {
        if (pipe->read_fds > 0)
            pipe->read_fds--;
        if (pipe->read_fds == 0) {
            pipe->read_node = NULL;
            notify_write_hup = true;
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

    if (notify_read_hup && read_node) {
        vfs_poll_notify(read_node, EPOLLHUP);
    }
    if (notify_write_hup && write_node) {
        vfs_poll_notify(write_node, EPOLLHUP);
    }

    if (free_spec) {
        if (spec->node)
            spec->node->handle = NULL;
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
    if (!spec)
        return 0;
    pipe_info_t *pipe = spec->info;
    if (!pipe)
        return 0;

    int out = 0;

    spin_lock(pipe->lock);
    if (!spec->write && !pipe->write_fds)
        out |= EPOLLHUP;
    if (events & EPOLLIN) {
        if (pipe->ptr > 0)
            out |= EPOLLIN;
    }

    if (spec->write && !pipe->read_fds)
        out |= EPOLLHUP;
    if (events & EPOLLOUT) {
        if (pipe->ptr < PIPE_BUFF)
            out |= EPOLLOUT;
    }
    spin_unlock(pipe->lock);
    return out;
}

int pipefs_mount(const char *handle, vfs_node_t node, void *data) {
    if (pipefs_root != NULL)
        return -EBUSY;
    node->fsid   = pipefs_id;
    pipefs_root  = node;
    node->handle = calloc(1, sizeof(pipe_specific_t));
    return EOK;
}

errno_t pipefs_stat(void *file, vfs_node_t node) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec)
        return EOK;
    pipe_info_t *pipe = spec->info;
    if (pipe == NULL)
        return EOK;
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
    if (pipefs_id == -EINVAL) {
        kerror("pipefs regist error.");
    }
}
