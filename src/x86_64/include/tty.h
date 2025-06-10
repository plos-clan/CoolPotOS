#pragma once

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

#include "ctype.h"
#include "lock_queue.h"

typedef enum tty_mode {
    CANONICAL = 0,
    RAW,
    ECHO,
} tty_mode_t;

typedef struct tty_virtual_device {
    void (*print)(struct tty_virtual_device *res, const char *string);
    void (*putchar)(struct tty_virtual_device *res, int c);

    uint64_t volatile *video_ram; // 显存基址
    uint64_t           width, height;
    tty_mode_t         mode;
} tty_t;

void   init_tty();
tty_t *alloc_default_tty();
void   free_tty(tty_t *tty);
void   build_tty_device();
tty_t *get_default_tty();
