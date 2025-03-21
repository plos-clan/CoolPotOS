#pragma once

#include "elf.h"
#include "module.h"

typedef int (*dlmain_t)(void);

typedef struct {
    char *name;
    void *addr;
} dlfunc_t;

void dlinker_load(cp_module_t *module);
void dlinker_init();
