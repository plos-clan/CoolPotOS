#pragma once

#if defined(__x86_64__) || defined(__amd64__)
#    include "cpu_features_x64.h"
#elif defined(__loongarch__) || defined(__loongarch64)
#    include "cpu_features_la64.h"
#elif defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    include "cpu_features_rv64.h"
#endif

#include "string_builder.h"

typedef struct {
    string_builder_t *features;
    char *vendor_id;
    char *model_name;
    unsigned int virt_bits;
    unsigned int phys_bits;
} cpu_features_t;

void arch_cpuid_feature_info(cpu_features_t *cpu_features); // 架构具体实现
cpu_features_t *get_global_features();
void cpu_features_setup();