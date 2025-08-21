#pragma once

#include "krlibc.h"
#include "module.h"

#define KERNEL_MODULES_SPACE_START 0xffffffffb0000000
#define KERNEL_MODULES_SPACE_END   0xffffffffc0000000

#define EXPORT_SYMBOL(name)                                                                        \
    __attribute__((used, section(".ksymtab"))) static const dlfunc_t __ksym_##name = {             \
        #name, (void *)name}

#define EXPORT_SYMBOL_F(func_name, name)                                                           \
    __attribute__((used, section(".ksymtab"))) static const dlfunc_t __ksym_##name = {             \
        #func_name, (void *)name}

typedef int (*dlinit_t)(void);

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

void module_setup();

void load_all_kernel_module();
