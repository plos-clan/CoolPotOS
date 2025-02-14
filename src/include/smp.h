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
}smp_cpu_t;

uint64_t get_current_cpuid();
void smp_setup();
void apu_startup(struct limine_smp_request smp_request);
uint64_t cpu_num();
