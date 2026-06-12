#pragma once

#include "lib/llist.h"

// 设备类型
enum device_type_t {
    DEV_NULL,  // 空设备
    DEV_CHAR,  // 字符设备
    DEV_BLOCK, // 块设备
    DEV_NET,   // 网络设备
};

// 设备子类型
enum device_subtype_t {
    DEV_TTY   = 4,   // TTY 设备
    DEV_PART  = 8,   // 磁盘分区
    DEV_SOUND = 116, // 声卡 / ALSA
    DEV_INPUT = 13,  // 输入设备
    DEV_FB    = 29,  // 帧缓冲
    DEV_DISK,        // 磁盘
    DEV_NETIF,       // 网卡
    DEV_SYSDEV,      // 系统设备
    DEV_USB,         // USB userspace node
    DEV_GPU = 226,   // 显卡
    DEV_MAX,
};

typedef struct device_t {
    struct llist_header node;

    char *name;      // 设备名
    int type;        // 设备类型
    int subtype;     // 设备子类型
    uint64_t dev;    // 设备号
    uint64_t parent; // 父设备号
    void *ptr;       // 设备指针

    ssize_t (*open)(void *dev, void *arg);
    ssize_t (*close)(void *dev, void *arg);
    // 设备控制
    ssize_t (*ioctl)(void *dev, int cmd, void *args);
    // 轮询
    ssize_t (*poll)(void *dev, int events);
    // 读设备
    ssize_t (*read)(void *dev, void *buf, uint64_t offset, size_t size);
    // 写设备
    ssize_t (*write)(void *dev, void *buf, uint64_t offset, size_t size);

    void *(*map)(void *dev, void *addr, size_t offset, size_t size, size_t prot);
} device_t;

struct device_install_ops {
    void *open;
    void *close;
    void *ioctl;
    void *poll;
    void *read;
    void *write;
    void *map;
};

uint64_t device_install(
    int type,
    int subtype,
    void *ptr,
    char *name,
    uint64_t parent,
    void *open,
    void *close,
    void *ioctl,
    void *poll,
    void *read,
    void *write,
    void *map
);

ssize_t device_write(uint64_t dev, void *buf, uint64_t idx, size_t count);

device_t *device_find(int subtype, uint64_t idx);
device_t *device_get(uint64_t dev);
void device_init();
