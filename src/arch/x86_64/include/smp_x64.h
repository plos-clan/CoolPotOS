#pragma once

#include "description_table.h"

typedef struct arch_cpu_{
    gdt_entries_t       gdtEntries;
    struct gdt_register gdt_pointer;
    tss_t               tss0;
    tss_stack_t         tss_stack;
}arch_cpu_t;
