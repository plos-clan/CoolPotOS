#pragma once

#define IPI_INIT    0x4500
#define IPI_STARTUP 0x4600

#include "acpi.h"
#include "description_table.h"
#include "krlibc.h"
#include "limine.h"
#include "lock_queue.h"
#include "page.h"
#include "pcb.h"

typedef struct smp_cpu {
    bool                ready;
    uint32_t            id;
    uint64_t            lapic_id;
    gdt_entries_t       gdtEntries;
    struct gdt_register gdt_pointer;
    tss_t               tss0;
    tss_stack_t         tss_stack;
    tcb_t               idle_pcb;
    tcb_t               current_pcb;
    page_directory_t   *directory;
    lock_queue         *death_queue;     // 死亡线程队列
    lock_queue         *scheduler_queue; // 该核心的线程调度队列
    lock_node          *iter_node;       // 该核心当前迭代的节点
} __attr(packed) smp_cpu_t;

extern smp_cpu_t smp_cpus[MAX_CPU];

/**
 * 获取当前CPUID
 * @return cpuid
 */
uint32_t   get_current_cpuid();
void       smp_setup();
uint64_t   cpu_num();
smp_cpu_t *get_cpu_smp(uint32_t processor_id);

static __attr(address_space(256)) smp_cpu_t *const cpu = (__attr(address_space(256)) void *)0;
