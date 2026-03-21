#pragma once

#define POLLIN  0x0001 // 有数据可读
#define POLLPRI 0x0002 // 有紧急数据可读（如 socket 的带外数据）
#define POLLOUT 0x0004 // 写操作不会阻塞（可写）

#define POLLERR  0x0008 // 错误（不需要设置，由内核返回）
#define POLLHUP  0x0010 // 挂起（对端关闭）
#define POLLNVAL 0x0020 // fd 无效（文件描述符非法）
#define POLLRDNORM 0x0040
#define POLLRDBAND 0x0080
#define POLLWRNORM 0x0100
#define POLLWRBAND 0x0200
#define POLLMSG    0x0400
#define POLLRDHUP  0x2000

#define EPOLLIN        0x001
#define EPOLLPRI       0x002
#define EPOLLOUT       0x004
#define EPOLLRDNORM    0x040
#define EPOLLNVAL      0x020
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400
#define EPOLLERR       0x008
#define EPOLLHUP       0x010
#define EPOLLRDHUP     0x2000
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP    (1U << 29)
#define EPOLLONESHOT   (1U << 30)
#define EPOLLET        (1U << 31)

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#include "types.h"
#include "lock.h"
#include "fs/vfs.h"

struct pollfd {
    int fd;
    short events;
    short revents;
};

// Linux-compatible epoll_event (packed on x86_64)
struct epoll_event {
    uint32_t events;
    uint64_t data;
} __attribute__((packed));

// Internal: one monitored fd entry
typedef struct epoll_entry {
    int fd;
    uint32_t events;
    uint64_t data;
} epoll_entry_t;

// Internal: epoll instance (stored in vfs_node->handle)
#define EPOLL_MAX_ENTRIES 128

typedef struct epoll_instance {
    epoll_entry_t entries[EPOLL_MAX_ENTRIES];
    int count;
    spin_t lock;
    vfs_node_t node;
} epoll_instance_t;

// eventfd flags (Linux ABI)
#define EFD_SEMAPHORE 00000001
#define EFD_CLOEXEC   02000000 // == O_CLOEXEC
#define EFD_NONBLOCK  00004000 // == O_NONBLOCK

typedef struct eventfd_ctx {
    uint64_t count;
    spin_t lock;
    vfs_node_t node;
    int flags;
} eventfd_ctx_t;

struct pollfd *
select_add(struct pollfd **comp, size_t *compIndex, size_t *complength, int fd, int events);
bool select_bitmap(const uint8_t *map, int index);
void select_bitmap_set(uint8_t *map, int index);
uint32_t poll_to_epoll_comp(uint32_t poll_events);
uint32_t epoll_to_poll_comp(uint32_t epoll_events);
void epollfs_regist();
void eventfdfs_regist();
