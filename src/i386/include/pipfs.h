#pragma once

#define FILE_BLKSIZE 4096

#include "krlibc.h"
#include "vfs.h"

typedef struct file {
    uint64_t size;
    uint8_t *blocks;
} *file_t;

void pipfs_update();
