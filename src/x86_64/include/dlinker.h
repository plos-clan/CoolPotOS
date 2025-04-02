#pragma once

#include "elf_util.h"
#include "module.h"

typedef int (*dlmain_t)(void);

typedef struct {
    char *name;
    void *addr;
} dlfunc_t;

/**
 * 加载一个内核模块
 * @param module 文件句柄
 */
void dlinker_load(cp_module_t *module);
void dlinker_init();
