#pragma once

#define IPI_INIT 0x4500
#define IPI_STARTUP 0x4600

#include "acpi.h"
#include "limine.h"
#include "description_table.h"

typedef struct smp_cpu{
    uint8_t flags; //标志位, CPU是否启用
    uint64_t lapic_id;
    gdt_entries_t gdtEntries;
    struct gdt_register gdt_pointer;
    tss_t tss0;
    tss_stack_t tss_stack;
}__attribute__((packed)) smp_cpu_t;

uint64_t get_current_cpuid();
void smp_setup();
void apu_startup(struct limine_smp_request smp_request);
uint64_t cpu_num();
