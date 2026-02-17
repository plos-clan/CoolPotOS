#pragma once

#include "types.h"

typedef struct module {
    char    *path;
    char     name[20];
    uint8_t *data;
    size_t   size;
} module_t;

module_t *get_module(const char *module_name);
module_t *get_module_raw(const char *module_name);
void      load_module();
