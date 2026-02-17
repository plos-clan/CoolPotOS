#pragma once

#include "description_table.h"

typedef struct arch_cpu_ {
    gdt_entries_t       gdtEntries;
    struct gdt_register gdt_pointer;
    tss_t               tss0;
    tss_stack_t         tss_stack;
    bool                support_tsc;
    uint32_t            tsc_conv_mul;
    uint32_t            tsc_conv_shift;
    uint64_t            tsc_base_tsc;
    uint64_t            tsc_base_ns;
} arch_cpu_t;

void calibrate_tsc_with_hpet();
