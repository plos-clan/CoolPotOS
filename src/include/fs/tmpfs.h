#pragma once

#include "fs/vfs.h"

enum tmpfs_type {
    tp_file_dir,
    tp_file_file,
    tp_file_symlink,
    tp_file_char,
    tp_file_blk,
    tp_file_socket,
};

typedef struct tmpfs_file {
    enum tmpfs_type type;
    char name[64];
    char *data;
    size_t size;
    size_t link_count;
    vfs_node_t node;
    vfs_node_t root;
    size_t capacity;
} tmpfs_file_t;

void tmpfs_regist();
