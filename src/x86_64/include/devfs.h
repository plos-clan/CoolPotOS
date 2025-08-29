#pragma once

#include "ctype.h"
#include "llist.h"
#include "vdisk.h"

typedef struct device_handle *device_handle_t;

struct device_handle {
    vdisk              *device; // 设备句柄
    char               *name;   // 节点名
    bool                is_dir; // 是否为目录
    struct llist_header child;  // 子节点链表
};

void   devfs_setup();
size_t devfs_read(void *file, void *addr, size_t offset, size_t size);
size_t devfs_write(void *file, const void *addr, size_t offset, size_t size);
