#pragma once

#include "types.h"

typedef enum module_state {
    M_LOADING,
    M_RUNNING,
    M_ERROR,
    M_NO_EXEC,
} module_state_t;

typedef struct module {
    char *path;
    char name[20];
    uint8_t *data;
    size_t size;
    module_state_t state;
} module_t;

module_t *get_module(const char *module_name);
module_t *get_module_raw(const char *module_name);
size_t get_modules_count();
module_t *get_modules_array();
void load_module();
