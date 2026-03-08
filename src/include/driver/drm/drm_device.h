#pragma once

#include "cow_arraylist.h"
#include "types.h"

enum device_type_t {
    DEV_NULL,  // 空设备
    DEV_CHAR,  // 字符设备
    DEV_BLOCK, // 块设备
    DEV_NET,   // 网络设备
};

typedef struct drmd_device_t {
    char *name;      // 设备名
    int type;        // 设备类型
    uint64_t dev;    // 设备号
    uint64_t parent; // 父设备号
    void *ptr;       // 设备指针
    size_t index;    // 列表索引
    // 设备控制
    int (*ioctl)(void *dev, size_t cmd, void *args);
    // 轮询
    int (*poll)(void *dev, size_t events);
    // 读设备
    size_t (*read)(void *dev, void *buf, uint64_t offset, size_t size);
    // 写设备
    size_t (*write)(void *dev, const void *buf, uint64_t offset, size_t size);

    void *(*map)(void *dev, void *addr, size_t offset, size_t size, size_t prot, size_t flags);
} drmd_device_t;

void drm_device_setup();
uint64_t drm_device_install(
    int type,
    void *ptr,
    char *name,
    uint64_t parent,
    void *ioctl,
    void *poll,
    void *read,
    void *write,
    void *map
);
size_t drm_size_t(void *data);
size_t drm_ioctl(void *data, size_t cmd, size_t arg);
size_t drm_read(void *data, void *buf, uint64_t offset, uint64_t len);
size_t drm_poll(void *data, size_t event);
void *drm_map(void *data, void *addr, uint64_t offset, uint64_t len);
cow_arraylist *drm_devices_get();
