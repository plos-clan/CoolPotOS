#pragma once

#include "types.h"

typedef struct module {
    char    *path;
    char     name[20];
    uint8_t *data;
    size_t   size;
} module_t;

void load_module();
