#pragma once

#include "krlibc.h"

typedef struct {
    bool     is_use;
    char     module_name[64];
    char    *path;
    uint8_t *data;
    size_t   size;
    char     raw_name[64];
} cp_module_t;

cp_module_t *get_module(const char *module_name);
