#pragma once

#include "types.h"

typedef struct block_device blk_device_t;

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
    char name[20];
    struct block_device_ops ops;
};

size_t device_read(size_t lba, size_t number, void *buffer, size_t blk_id);
size_t device_write(size_t lba, size_t number, const void *buffer, size_t blk_id);

errno_t delete_blk_device(size_t blk_id);
size_t register_device(blk_device_t *device);

void init_block_device_manager();
