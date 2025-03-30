#pragma once

#include "ctype.h"
#include "limine.h"

typedef struct {
    bool     is_use;
    char     module_name[20];
    char    *path;
    uint8_t *data;
    size_t   size;
} cp_module_t;

void module_setup();

/**
 * 根据module_name获取模块
 * @param module_name 模块名
 * @return NULL ? 未找到 : 模块指针
 */
cp_module_t *get_module(const char *module_name);
