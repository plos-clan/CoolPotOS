#pragma once

#include "ctype.h"

typedef struct {
    char *vendor;
    char model_name[64];
    unsigned int virt_bits;
    unsigned int phys_bits;
} cpu_t;

void init_cpuid();

cpu_t get_cpu_info();
