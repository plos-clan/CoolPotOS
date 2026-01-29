#pragma once

#define PIPE_BUFF 8192 // 管道缓冲区大小

#include "lock.h"
#include "task/task.h"
#include "vfs.h"

typedef struct task_block_list {
    struct task_block_list *next;
    tcb_t                   thread;
} task_block_list_t;

typedef struct pipe_info {
    uint32_t ptr;
    char    *buf;
    int      assigned;

    int write_fds;
    int read_fds;
    int active;
    bool free_pending;

    spin_t lock;

    task_block_list_t blocking_read;
    task_block_list_t blocking_write;
} pipe_info_t;

typedef struct pipe_specific pipe_specific_t;
struct pipe_specific {
    bool         write;
    pipe_info_t *info;
    vfs_node_t   node;
    int          active;
    bool         free_pending;
};

void pipefs_regist();
