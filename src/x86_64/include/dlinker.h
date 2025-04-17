#pragma once

#define EXPORT_SYMBOL(name)                                                                        \
    __attribute__((used, section(".ksymtab"))) static const dlfunc_t __ksym_##name = {             \
        #name, (void *)name}

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

dlfunc_t *find_func(const char *name);

void find_kernel_symbol();

void dlinker_init();
