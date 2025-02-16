#pragma once

#include "ctype.h"
#include "list.h"
#include "pcb.h"

typedef enum {
    DIR,
    FILE,
}pivfs_file_type;

typedef struct pipfs_handle {
    pivfs_file_type type;
    char* path;
    uint64_t size;
    uint8_t *blocks;
    list_t child;
} pivfs_handle_t;

void pivfs_setup();
void pivfs_update(pcb_t kernel_head);
