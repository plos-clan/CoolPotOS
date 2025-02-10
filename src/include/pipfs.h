#pragma once

#include "ctype.h"
#include "list.h"
#include "pcb.h"

typedef enum {
    DIR,
    FILE,
}pipfs_file_type;

typedef struct pipfs_handle {
    pipfs_file_type type;
    char* path;
    uint64_t size;
    uint8_t *blocks;
    list_t child;
} pipfs_handle_t;

void pipfs_setup();
void pipfs_update(pcb_t kernel_head);
