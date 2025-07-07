#pragma once

#define POLLIN  0x0001 // 有数据可读
#define POLLPRI 0x0002 // 有紧急数据可读（如 socket 的带外数据）
#define POLLOUT 0x0004 // 写操作不会阻塞（可写）

#define POLLERR  0x0008 // 错误（不需要设置，由内核返回）
#define POLLHUP  0x0010 // 挂起（对端关闭）
#define POLLNVAL 0x0020 // fd 无效（文件描述符非法）

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

#include "ctype.h"

struct pollfd {
    int   fd;
    short events;
    short revents;
};

struct pollfd *select_add(struct pollfd **comp, size_t *compIndex, size_t *complength, int fd,
                          int events);
bool           select_bitmap(uint8_t *map, int index);
void           select_bitmap_set(uint8_t *map, int index);
uint32_t       poll_to_epoll_comp(uint32_t poll_events);
uint32_t       epoll_to_poll_comp(uint32_t epoll_events);
