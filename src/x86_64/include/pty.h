#pragma once

#define PTY_BUFF_SIZE 4096

#include "errno.h"
#include "llist.h"
#include "tty.h"
#include "vfs.h"

typedef struct pty_handle {
    int                 id;
    vfs_node_t          node;
    struct termios      term;
    struct winsize      win;
    struct pty_handle  *slave;
    struct vt_mode      vt_mode;
    spin_t              lock;
    pid_t               ctrl_pgid;
    int                 master_fds;
    int                 slave_fds;
    int                 tty_kbmode;
    int                 ptr_master;
    int                 ptr_slave;
    bool                locked;
    uint8_t            *master_buffer;
    uint8_t            *slave_buffer;
    struct llist_header list_node;
} pty_handle_t;

void pty_init();
