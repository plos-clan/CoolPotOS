#pragma once

#include "cptype.h"

typedef struct tmpfs_file {
    bool               is_dir;
    char               name[64];
    struct tmpfs_file *parent;
    struct tmpfs_file *children[64];
    size_t             child_count;
    char              *data;
    size_t             size;
    size_t             capacity;
    int                ready;
} tmpfs_file_t;

void tmpfs_setup();
