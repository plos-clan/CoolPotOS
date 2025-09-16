#pragma once

#define CPFS_MAX_FILE 256

#include "ctype.h"
#include "llist.h"

typedef struct cpfs_file {
    bool                is_dir;
    char                name[64];
    struct cpfs_file   *parent;
    struct cpfs_file   *children[CPFS_MAX_FILE];
    size_t              child_count;
    char               *data;
    size_t              size;
    size_t              capacity;
    int                 ready;
    struct llist_header curr_node;
    struct llist_header child_node;
} cpfs_file_t;

void cpfs_setup();
