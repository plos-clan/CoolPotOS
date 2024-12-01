#pragma once

#define TTY_VGA_OUTPUT 2 //VGA内核日志输出
#define TTY_VBE_OUTPUT 4 //VBE内核日志输出
#define TTY_OST_OUTPUT 8 //基于os_terminal的虚拟终端内核日志输出
#define TTY_LOG_OUTPUT 16//后台内核日志

#include "ctypes.h"
#include "fifo8.h"

typedef enum {
    MODE_A = 'A',
    MODE_B = 'B',
    MODE_C = 'C',
    MODE_D = 'D',
    MODE_E = 'E',
    MODE_F = 'F',
    MODE_G = 'G',
    MODE_H = 'H',
    MODE_f = 'f',
    MODE_J = 'J',
    MODE_K = 'K',
    MODE_S = 'S',
    MODE_T = 'T',
    MODE_m = 'm'
} vt100_mode_t;

typedef struct tty_device {
    void (*print)(struct tty_device *res, const char *string);
    void (*putchar)(struct tty_device *res, int c);
    void (*clear)(struct tty_device *res);
    void (*move_cursor)(int x, int y);

    uint32_t volatile *video_ram; // 显存基址
    uint32_t width, height;
    int x, y;
    uint32_t color, back_color;
    struct FIFO8 *fifo; //键盘缓冲队列
    char name[50];
} tty_t;

tty_t *default_tty_alloc();
void free_tty(tty_t *tty);
void tty_init(void);
