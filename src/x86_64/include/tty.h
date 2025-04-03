#pragma once

#include "lock_queue.h"
#include "ctype.h"

typedef struct tty_virtual_device {
    void (*print)(struct tty_virtual_device *res, const char *string);
    void (*putchar)(struct tty_virtual_device *res, int c);

    uint64_t volatile *video_ram; // 显存基址
    uint64_t           width, height;
    lock_queue        *keyboard_buffer;
    bool               is_key_wait; // 是否等待键盘输入
} tty_t;

void   init_tty();
tty_t *alloc_default_tty();
void   free_tty(tty_t *tty);
tty_t *get_default_tty();
