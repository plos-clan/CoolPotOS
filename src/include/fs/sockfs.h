#pragma once

#include "lock.h"
#include "vfs.h"

#define SOCK_BUFF 16384

// Address families
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX

// Socket types
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC 02000000

// Shutdown how
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

// Socket options
#define SOL_SOCKET 1

#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_DOMAIN 39
#define SO_PASSCRED 16
#define SO_PEERCRED 17

// MSG flags
#define MSG_PEEK 0x02
#define MSG_DONTWAIT 0x40

// UNIX_PATH_MAX
#define UNIX_PATH_MAX 108

// PLACEHOLDER_CONTINUE

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

struct msghdr {
    void *msg_name;
    uint32_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
};

typedef enum {
    SS_UNCONNECTED = 0,
    SS_BOUND,
    SS_LISTENING,
    SS_CONNECTING,
    SS_CONNECTED,
    SS_DISCONNECTING,
} socket_state_t;

typedef struct sock_ringbuf {
    char *buf;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} sock_ringbuf_t;

typedef struct socket_info {
    int domain;
    int type;
    int protocol;
    socket_state_t state;
    int so_error;

    // AF_UNIX addressing
    char bound_path[UNIX_PATH_MAX];
    bool is_bound;
    vfs_node_t bound_node;

    // SOCK_STREAM connection
    struct socket_info *peer;

    // listen/accept queue
    int backlog;
    int pending_count;
    struct socket_info **pending_queue;
    bool accept_waiting;

    // Data buffer
    sock_ringbuf_t recv_buf;

    // Reference counting and synchronization
    int refcount;
    int active;
    bool free_pending;
    spin_t lock;
} socket_info_t;

typedef struct socket_specific {
    socket_info_t *info;
    vfs_node_t node;
    int active;
    bool free_pending;
    bool shut_rd;
    bool shut_wr;
} socket_specific_t;

// Ring buffer operations
void ringbuf_init(sock_ringbuf_t *rb, size_t capacity);
size_t ringbuf_read(sock_ringbuf_t *rb, void *dst, size_t len);
size_t ringbuf_write(sock_ringbuf_t *rb, const void *src, size_t len);
size_t ringbuf_peek(sock_ringbuf_t *rb, void *dst, size_t len);
void ringbuf_destroy(sock_ringbuf_t *rb);

// sockfs registration
void sockfs_regist();

// Helper to create a socket VFS node
vfs_node_t sockfs_create_node(socket_info_t *info);
