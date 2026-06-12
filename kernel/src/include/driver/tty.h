#pragma once

#include "term/termios.h"
#include "term/vtmode.h"
#include "lib/llist.h"

typedef struct tty_session tty_t;
typedef struct tty_session_ops tty_session_ops_t;
typedef struct tty_device tty_device_t;

enum tty_device_type {
    TTY_DEVICE_SERIAL = 0, // 串口设备
    TTY_DEVICE_GRAPHI = 1, // 图形设备
};

typedef struct tty_device_ops {
    size_t (*write)(tty_device_t *device, const char *buf, size_t count);
    size_t (*read)(tty_device_t *device, char *buf, size_t count);
    void (*flush)(tty_device_t *res);
    int (*ioctl)(tty_device_t *device, uint32_t cmd, uint32_t arg);
} tty_device_ops_t;

struct tty_session_ops {
    size_t (*write)(tty_t *device, const char *buf, size_t count);
    size_t (*read)(tty_t *device, char *buf, size_t count);
    void (*flush)(tty_t *res);
    int (*ioctl)(tty_t *device, uint32_t cmd, uint64_t arg);
    int (*poll)(tty_t *device, int events);
};

struct tty_graphics_ {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

struct tty_serial_ {
    uint16_t port;
};

struct tty_device {
    enum tty_device_type type;
    tty_device_ops_t ops; // 图形设备不具备 read write 操作
    void *private_data;   // 实际设备
    char name[32];

    struct llist_header node;
};

struct tty_session {
    void *terminal;
    termios term;
    vt_mode vtmode;
    int tty_kbmode;
    int tty_mode;
    pid_t pgid;
    tty_session_ops_t ops;
    tty_device_t *device; // 会话所属的TTY设备
};

tty_device_t *alloc_tty_device(enum tty_device_type type);
uint64_t register_tty_device(tty_device_t *device);
uint64_t delete_tty_device(tty_device_t *device);
tty_device_t *get_tty_device(const char *name);
tty_t *get_kernel_session();
void tty_init();
