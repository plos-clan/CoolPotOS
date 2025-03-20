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

void         module_setup();
cp_module_t *get_module(const char *module_name);
