#pragma once

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
tty_t *get_default_tty();
