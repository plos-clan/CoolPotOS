#pragma once

#include "fs/vfs.h"

enum devtmpfs_type {
    dtp_file_dir,
    dtp_file_file,
    dtp_file_symlink,
    dtp_file_device,
};

enum device_type{
    device_stream = 2,
    device_block = 4,
};

typedef struct devtmp_handle {
    enum devtmpfs_type type;

    char       name[64];
    vfs_node_t node;
    vfs_node_t root;
    size_t     capacity;
    void      *data;
    size_t     size;

    void *device_handle;
    enum device_type dev_type;
    errno_t (*ioctl_t)(void *handle, size_t req, void *arg);
    size_t (*read_t)(void *handle, void *addr, size_t offset, size_t size);
    size_t (*write_t)(void *handle, const void *addr, size_t offset, size_t size);
    errno_t (*poll_t)(void *handle, size_t events);
    size_t (*size_t)(void *handle);
    void *(*mapfile_t)(void *file, void *addr, size_t offset, size_t size, size_t prot,
                           size_t flags);
} dtmp_handle_t;

errno_t create_device_node(vfs_node_t root, char *name, enum device_type type, void *handle,
                           vfs_ioctl_t ioctl, vfs_read_t read, vfs_write_t write, vfs_poll_t poll,
                           vfs_mapfile_t map,size_t (*size_t)(void *handle));
void devtmpfs_regist();
