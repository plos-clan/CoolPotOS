#pragma once

#include "driver/tty.h"
#include "fs/vfs.h"
#include "lock.h"

#define PTY_BUF_SIZE 4096
#define PTY_MAX_COUNT 256

// PTY ring buffer for bidirectional communication
typedef struct pty_ringbuf {
    char *buf;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} pty_ringbuf_t;

// PTY pair structure
typedef struct pty_pair {
    int index;       // PTY index number
    int locked;      // Whether slave is locked (TIOCSPTLCK)
    int master_open; // Master open reference count
    int slave_open;  // Slave open reference count
    spin_t lock;     // Spinlock for synchronization

    // Bidirectional buffers
    pty_ringbuf_t master_to_slave; // Data from master to slave
    pty_ringbuf_t slave_to_master; // Data from slave to master

    vfs_node_t master_node; // Master VFS node
    vfs_node_t slave_node;  // Slave VFS node

    termios_t termios;      // Terminal attributes
    struct winsize winsize; // Window size

    pid_t foreground_pgid; // Foreground process group
    pid_t session_id;      // Controlling session

    int refcount; // Reference count
} pty_pair_t;

// PTY specific data for file handles
typedef struct pty_specific {
    pty_pair_t *pair;
    bool is_master; // true for master, false for slave
    bool active;
} pty_specific_t;

// Initialize PTY subsystem
void pty_init();

// Allocate a new PTY pair (called when opening /dev/ptmx)
pty_pair_t *pty_alloc();

// Free a PTY pair
void pty_free(pty_pair_t *pair);

// Get PTY pair by index
pty_pair_t *pty_get(int index);

// Ring buffer operations
void pty_ringbuf_init(pty_ringbuf_t *rb, size_t capacity);
size_t pty_ringbuf_read(pty_ringbuf_t *rb, void *dst, size_t len);
size_t pty_ringbuf_write(pty_ringbuf_t *rb, const void *src, size_t len);
size_t pty_ringbuf_available(pty_ringbuf_t *rb);
void pty_ringbuf_destroy(pty_ringbuf_t *rb);

// Register ptmx device in devtmpfs
void ptmx_init();
