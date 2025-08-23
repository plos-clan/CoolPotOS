#pragma once

#define NCCS 32

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

#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

#define CSIZE  0000060
#define CS5    0000000
#define CS6    0000020
#define CS7    0000040
#define CS8    0000060
#define CSTOPB 0000100
#define CREAD  0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL  0002000
#define CLOCAL 0004000

#define ISIG   0000001
#define ICANON 0000002
#define ECHO   0000010
#define ECHOE  0000020
#define ECHOK  0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define TOSTOP 0000400
#define IEXTEN 0100000

#define KDGETMODE   0x4B3B // 获取终端模式命令
#define KDSETMODE   0x4B3A // 设置终端模式命令
#define KD_TEXT     0x00   // 文本模式
#define KD_GRAPHICS 0x01   // 图形模式

#define KDGKBMODE   0x4B44 /* gets current keyboard mode */
#define KDSKBMODE   0x4B45 /* sets current keyboard mode */
#define K_RAW       0x00   // 原始模式（未处理扫描码）
#define K_XLATE     0x01   // 转换模式（生成ASCII）
#define K_MEDIUMRAW 0x02   // 中等原始模式
#define K_UNICODE   0x03   // Unicode模式

#define VT_OPENQRY 0x5600 /* get next available vt */
#define VT_GETMODE 0x5601 /* get mode of active vt */
#define VT_SETMODE 0x5602

#define VT_GETSTATE 0x5603
#define VT_SENDSIG  0x5604

#define VT_ACTIVATE   0x5606 /* make vt active */
#define VT_WAITACTIVE 0x5607 /* wait for vt active */

#define VT_AUTO    0x00 // 自动切换模式
#define VT_PROCESS 0x01 // 进程控制模式

#include "ctype.h"
#include "lock_queue.h"

struct vt_state {
    uint16_t v_active; // 活动终端号
    uint16_t v_state;  // 终端状态标志
};

struct vt_mode {
    char  mode;   // 终端模式
    char  waitv;  // 垂直同步
    short relsig; // 释放信号
    short acqsig; // 获取信号
    short frsig;  // 强制释放信号
};

typedef struct termios {
    uint32_t c_iflag;    /* input mode flags */
    uint32_t c_oflag;    /* output mode flags */
    uint32_t c_cflag;    /* control mode flags */
    uint32_t c_lflag;    /* local mode flags */
    uint8_t  c_line;     /* line discipline */
    uint8_t  c_cc[NCCS]; /* control characters */
} termios_t;

typedef struct tty_virtual_device {
    void (*print)(struct tty_virtual_device *res, const char *string);
    void (*putchar)(struct tty_virtual_device *res, int c);
    void (*flush)(struct tty_virtual_device *res);

    uint64_t volatile *video_ram; // 显存基址
    uint64_t           width, height;
    termios_t          termios;
    bool               is_sigterm; // 是否为前台进程组终端
} tty_t;

void   init_tty();
tty_t *alloc_default_tty();
void   free_tty(tty_t *tty);
void   build_tty_device();
tty_t *get_default_tty();

int  kernel_getch();
void keyboard_tmp_writer(const uint8_t *data);
