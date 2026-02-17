#pragma once

#include "fs/vfs.h"

typedef struct sysfs_handle {
    char name[64];
    char *data;
    size_t size;
    size_t capacity;
    vfs_node_t node;
} sysfs_handle_t;

void sysfs_regist();
vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, bool is_dir);
vfs_node_t sysfs_child_append_symlink(vfs_node_t parent, const char *name, const char *target);
vfs_node_t sysfs_regist_dev(
    char type, int major, int minor, const char *bus_path, const char *dev_name,
    const char *uevent_content);
