#pragma once

#define FD_INITIAL_CAPACITY 5 // 文件描述符表初始容量
#define FD_GROWTH_FACTOR 2    // 扩容因子
#define FD_START_INDEX 3      // 0, 1, 2 默认被 STDIN, STDOUT, STDERR 占用

#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DSYNC 010000
#define O_SYNC 04010000
#define O_RSYNC 04010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW 0400000
#define O_CLOEXEC 02000000

#define O_ASYNC 020000
#define O_DIRECT 040000
#define O_LARGEFILE 0100000
#define O_NOATIME 01000000
#define O_PATH 010000000
#define O_TMPFILE 020200000
#define O_NDELAY O_NONBLOCK

#include "fs/vfs.h"

typedef struct file_description {
    vfs_node_t node;
    size_t     offset;
    vfs_node_t dir_last;
    uint64_t   flags;
    int        fd;
} fd_t;

typedef struct file_description_table {
    fd_t **fds;        // 文件描述符表
    size_t fds_length; // 文件描述符表长度
} fdt_t;

fd_t   *fd_dup(fd_t *src);
fdt_t  *copy_fdt(fdt_t *src_fdt);
int     find_free_fd(fdt_t *pcb);
errno_t remove_fd(fdt_t *fdt, int fd);
errno_t set_fd(fdt_t *table, fd_t *handle, int fd);
int     add_fd(fdt_t *fdt, fd_t *new_fd);
fd_t   *get_fd(fdt_t *table, int fd);
fdt_t  *fds_init();
void    free_fdt(fdt_t *fdt);
