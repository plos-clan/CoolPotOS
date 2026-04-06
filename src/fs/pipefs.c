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

static void pipefs_enter(pipe_specific_t *spec) {
    if (spec == NULL || spec->info == NULL) {
        return;
    }

    pipe_info_t *pipe = spec->info;
    spin_lock(pipe->lock);
    spec->active++;
    pipe->active++;
    spin_unlock(pipe->lock);
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

    pipefs_enter(spec);

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

        if (spec->node && (spec->node->flags & O_NONBLOCK)) {
            spin_unlock(pipe->lock);
            ret = -EWOULDBLOCK;
            goto out;
        }

        spin_unlock(pipe->lock);

        vfs_poll_wait_t wait;
        vfs_poll_wait_init(&wait, get_current_task(), EPOLLIN | EPOLLHUP | EPOLLERR);
        vfs_poll_wait_arm(spec->node, &wait);
        int reason = vfs_poll_wait_sleep(spec->node, &wait, -1, "pipe_read");
        vfs_poll_wait_disarm(&wait);
        if (reason != EOK) {
            ret = -EINTR;
            goto out;
        }
    }

out:
    pipefs_leave(spec);
    return ret;
}

static size_t pipe_write_inner(
    pipe_specific_t *spec, const void *addr, size_t size, bool atomic, bool allow_wait
) {
    pipe_info_t *pipe = spec->info;

    while (true) {
        spin_lock(pipe->lock);

        if (pipe->read_fds == 0) {
            spin_unlock(pipe->lock);
            return -EPIPE;
        }

        size_t available = PIPE_BUFF - pipe->ptr;
        if (available > 0 && (!atomic || available >= size)) {
            size_t to_write = atomic ? size : MIN(size, available);
            memcpy(&pipe->buf[pipe->ptr], addr, to_write);
            pipe->ptr += to_write;
            pipe->assigned = (int)pipe->ptr;
            pipefs_update_nodes(pipe);
            spin_unlock(pipe->lock);
            if (pipe->read_node)
                vfs_poll_notify(pipe->read_node, EPOLLIN);
            return to_write;
        }

        spin_unlock(pipe->lock);

        if (!allow_wait)
            return -EWOULDBLOCK;

        vfs_poll_wait_t wait;
        vfs_poll_wait_init(&wait, get_current_task(), EPOLLOUT | EPOLLHUP | EPOLLERR);
        vfs_poll_wait_arm(spec->node, &wait);
        int reason = vfs_poll_wait_sleep(spec->node, &wait, -1, "pipe_write");
        vfs_poll_wait_disarm(&wait);
        if (reason != EOK)
            return -EINTR;
    }
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec || spec->info == NULL || !spec->write) {
        return (size_t)-1;
    }

    const char *data = addr;
    size_t written   = 0;
    // POSIX only requires atomicity for short pipe writes.
    const bool atomic   = size <= PIPE_ATOMIC_MAX;
    const bool nonblock = spec->node && (spec->node->flags & O_NONBLOCK);

    pipefs_enter(spec);

    while (written < size) {
        ssize_t ret = pipe_write_inner(spec, data + written, size - written, atomic, !nonblock);
        if (ret < 0)
            break;
        if (ret == 0)
            break;

        written += ret;
    }

    pipefs_leave(spec);
    if (written != 0) {
        return written;
    }
    return (size_t)-EWOULDBLOCK;
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

    bool free_spec        = false;
    bool free_pipe        = false;
    bool notify_read_hup  = false;
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
            notify_read_hup  = true;
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
            pipe->read_node  = NULL;
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
        vfs_poll_notify(write_node, EPOLLERR);
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
    if (!spec->write) {
        const bool eof = pipe->write_fds == 0;
        if (eof) {
            out |= EPOLLHUP;
        }
        if ((events & EPOLLIN) && (pipe->ptr > 0 || eof)) {
            out |= EPOLLIN;
        }
    } else {
        const bool broken = pipe->read_fds == 0;
        if (broken) {
            out |= EPOLLERR;
        }
        if ((events & EPOLLOUT) && !broken && pipe->ptr < PIPE_BUFF) {
            out |= EPOLLOUT;
        }
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
