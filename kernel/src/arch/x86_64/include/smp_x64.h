#pragma once

#include "types.h"
#include "cpu/description_table.h"

typedef struct x86_64_local_info {
    size_t lapic_id;
    gdt_entries_t gdtEntries;
    struct gdt_register gdt_pointer;
    tss_t tss0;
    bool support_tsc;
    uint32_t tsc_conv_mul;
    uint32_t tsc_conv_shift;
    uint64_t tsc_base_tsc;
    uint64_t tsc_base_ns;
} x86_64_local_info_t;
