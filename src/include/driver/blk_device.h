#pragma once

#include "types.h"

typedef struct block_device blk_device_t;

enum blk_type {
    BLK_BLOCK_DEVICE,
    BLK_PARTITION,
};

struct block_device_ops {
    size_t (*read)(void *handle, uint8_t *buffer, size_t number, size_t lba);
    size_t (*write)(void *handle, uint8_t *buffer, size_t number, size_t lba);
    int (*ioctl)(blk_device_t *device, size_t req, void *handle);
    int (*poll)(size_t events);
    void *(*map)(void *handle, void *addr, size_t len);
    errno_t (*del_blk)(void *handle);
};

struct block_device {
    void  *handle;
    size_t device_id;
    size_t size;       // 块设备大小
    size_t block_size; // 块大小
    size_t max_size;   // 最大读取缓冲区
    char   name[20];

    enum blk_type           type;
    struct block_device_ops ops;
};

size_t blk_device_read(blk_device_t *device, void *buffer, size_t offset, size_t length);
size_t blk_device_write(blk_device_t *device, const void *buffer, size_t offset, size_t length);

errno_t delete_blk_device(size_t blk_id);
size_t  register_device(blk_device_t *device);

void init_block_device_manager();
