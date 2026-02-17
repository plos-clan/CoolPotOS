#pragma once

#include "types.h"

typedef struct arch_cpu_ {

} arch_cpu_t;

uint64_t hartid_to_cpuid(uint64_t hartid);
uint64_t cpuid_to_hartid(uint64_t cpuid);
