#pragma once

#define KERNEL_MODULES_SPACE_START 0xffffffffb0000000
#define KERNEL_MODULES_SPACE_END   0xffffffffc0000000

#include "elf_load.h"
#include "types.h"

typedef int (*dlinit_t)(void);

typedef struct {
    char *name;
    void *addr;
} dlfunc_t;

typedef struct kernel_mode {
    size_t data_len;
    void *data;
    dlinit_t entry;
    dlinit_t task_entry;
    int entry_exit_code;
    dlfunc_t **export_funcs;
    size_t export_count;
    size_t lists_index;
    char *name;
} kernel_mode_t;

void start_all_kernel_module();
void kmodule_init();
