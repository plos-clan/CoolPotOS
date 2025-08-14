#pragma once

#define CPUINFO_X2APIC (1ULL << 1)
#define CPUINFO_RDTSC  (1ULL << 2)
#define CPUINFO_MMX    (1ULL << 4)
#define CPUINFO_SSE    (1ULL << 5)

#include "ctype.h"

typedef struct {
    char        *vendor;
    char         model_name[64];
    unsigned int virt_bits;
    unsigned int phys_bits;
} cpu_t;

void  init_cpuid();
bool  cpuid_has_sse();
bool  cpu_has_rdtsc();
cpu_t get_cpu_info();
