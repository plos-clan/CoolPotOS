#pragma once

#include "elf.h"
#include "module.h"

typedef int (*dlmain_t)(void);

void dlinker_load(cp_module_t *module);
