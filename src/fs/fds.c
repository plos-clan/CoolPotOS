#include "fs/fds.h"
#include "errno.h"
#include "fs/pipefs.h"
#include "fs/sockfs.h"
#include "fs/vfs.h"

int find_free_fd(fdt_t *fdt) {
    if (!fdt) { return -ENOENT; }
    for (size_t i = 0; i < fdt->fds_length; i++) {
        if (fdt->fds[i] == NULL) { return (int)i; }
    }
    return -EBADF;
}

static int expand_fds_table(fdt_t *fdt) {
    size_t old_len = fdt->fds_length;
    size_t new_len = old_len * FD_GROWTH_FACTOR;

    if (new_len == 0) { new_len = FD_INITIAL_CAPACITY; }

    fd_t **new_fds = (fd_t **)realloc(fdt->fds, new_len * sizeof(fd_t *));
    if (new_fds == NULL) { return -1; }

    fdt->fds            = new_fds;
    size_t new_elements = new_len - old_len;
    memset(&fdt->fds[old_len], 0, new_elements * sizeof(fd_t *));

    fdt->fds_length = new_len;
    return 0;
}

int add_fd(fdt_t *fdt, fd_t *new_fd) {
    int fdid = find_free_fd(fdt);
    if (fdid < 0) {
        if (expand_fds_table(fdt) != 0) { return -1; }
        fdid = find_free_fd(fdt);
        if (fdid < 0) { return -1; }
    }
    fdt->fds[fdid] = new_fd;
    return fdid;
}

fd_t *get_fd(fdt_t *table, int fd) {
    if (fd >= table->fds_length) return NULL;
    return table->fds[fd];
}

errno_t set_fd(fdt_t *table, fd_t *handle, int fd) {
    if (table->fds[fd] != NULL) return -EEXIST;
    table->fds[fd] = handle;
    return EOK;
}

errno_t remove_fd(fdt_t *fdt, int fd) {
    if (!fdt) { return -ENOENT; }
    if (fd < 0 || (size_t)fd >= fdt->fds_length) { return -EBADF; }
    fd_t *fd_to_remove = fdt->fds[fd];
    if (fd_to_remove == NULL) { return -EBADF; }
    fdt->fds[fd] = NULL;
    free(fd_to_remove);
    return EOK;
}

void free_fdt(fdt_t *fdt) {
    free((void *)fdt->fds);
    free(fdt);
}

fd_t *fd_dup(fd_t *src) {
    fd_t *new = (fd_t *)malloc(sizeof(fd_t));
    not_null_assert(new, "fd_dup out of memory.");
    src->node->refcount++;
    new->node       = src->node;
    new->offset     = src->offset;
    new->dir_last   = src->dir_last;
    new->flags      = src->flags;
    new->fd         = src->fd;
    vfs_node_t node = new->node;
    if (node->type & file_pipe) {
        pipe_specific_t *spec = node->handle;
        pipe_info_t     *pipe = spec->info;
        spin_lock(pipe->lock);
        if (spec->write) {
            pipe->write_fds++;
        } else {
            pipe->read_fds++;
        }
        spin_unlock(pipe->lock);
    }
    if (node->type & file_socket) {
        socket_specific_t *spec = node->handle;
        if (spec && spec->info) {
            spin_lock(spec->info->lock);
            spec->info->refcount++;
            spin_unlock(spec->info->lock);
        }
    }
    return new;
}

fdt_t *copy_fdt(fdt_t *src_fdt) {
    if (!src_fdt) { return NULL; }

    fdt_t *new_fdt = (fdt_t *)malloc(sizeof(fdt_t));
    if (new_fdt == NULL) { return NULL; }

    new_fdt->fds_length = src_fdt->fds_length;

    size_t array_size = new_fdt->fds_length * sizeof(fd_t *);
    new_fdt->fds      = (fd_t **)malloc(array_size);
    if (new_fdt->fds == NULL) {
        free(new_fdt);
        return NULL;
    }
    for (size_t i = 0; i < src_fdt->fds_length; i++) {
        fd_t *fd_entry = src_fdt->fds[i];
        if (fd_entry != NULL) {
            new_fdt->fds[i] = fd_dup(fd_entry);
        } else {
            new_fdt->fds[i] = NULL;
        }
    }
    return new_fdt;
}

fdt_t *fds_init() {
    fdt_t *fdt      = malloc(sizeof(fdt_t));
    fdt->fds_length = FD_INITIAL_CAPACITY;
    fdt->fds        = (fd_t **)malloc(fdt->fds_length * sizeof(fd_t *));
    if (fdt->fds) {
        memset(fdt->fds, 0, fdt->fds_length * sizeof(fd_t *));
    } else {
        fdt->fds_length = 0;
    }
    return fdt;
}
