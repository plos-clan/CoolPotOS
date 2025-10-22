#include "fs/fds.h"
#include "errno.h"

int find_free_fd(fdt_t *fdt) {
    if (!fdt) {
        return -ENOENT;
    }
    for (size_t i = 0; i < fdt->fds_length; i++) {
        if (fdt->fds[i] == NULL) {
            return (int)i;
        }
    }
    return -EBADF;
}

static int expand_fds_table(fdt_t *fdt) {
    size_t old_len = fdt->fds_length;
    size_t new_len = old_len * FD_GROWTH_FACTOR;

    if (new_len == 0) {
        new_len = FD_INITIAL_CAPACITY;
    }

    fd_t **new_fds = (fd_t **)realloc(fdt->fds, new_len * sizeof(fd_t *));
    if (new_fds == NULL) {
        return -1;
    }

    fdt->fds = new_fds;
    size_t new_elements = new_len - old_len;
    memset(&fdt->fds[old_len], 0, new_elements * sizeof(fd_t *));

    fdt->fds_length = new_len;
    return 0;
}

int add_fd(fdt_t *fdt, fd_t *new_fd) {
    int fdid = find_free_fd(fdt);
    if (fdid < 0) {
        if (expand_fds_table(fdt) != 0) {
            return -1;
        }
        fdid = find_free_fd(fdt);
        if (fdid < 0) {
            return -1;
        }
    }
    fdt->fds[fdid] = new_fd;
    return fdid;
}

fd_t *get_fd(fdt_t *table,int fd){
    if(fd >= table->fds_length) return NULL;
    return table->fds[fd];
}

errno_t set_fd(fdt_t *table,fd_t *handle,int fd){
    if(table->fds[fd] != NULL) return -EEXIST;
    table->fds[fd] = handle;
    return EOK;
}

errno_t remove_fd(fdt_t *fdt, int fd) {
    if (!fdt) {
        return -ENOENT;
    }
    if (fd < 0 || (size_t)fd >= fdt->fds_length) {
        return -EBADF;
    }
    fd_t *fd_to_remove = fdt->fds[fd];
    if (fd_to_remove == NULL) {
        return -EBADF;
    }
    fdt->fds[fd] = NULL;
    free(fd_to_remove);
    return EOK;
}

fdt_t *fds_init() {
    fdt_t *fdt = malloc(sizeof(fdt_t));
    fdt->fds_length = FD_INITIAL_CAPACITY;
    fdt->fds = (fd_t **)malloc(fdt->fds_length * sizeof(fd_t *));
    if (fdt->fds) {
        memset(fdt->fds, 0, fdt->fds_length * sizeof(fd_t *));
    } else {
        fdt->fds_length = 0;
    }
    return fdt;
}
