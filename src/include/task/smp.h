#pragma once

#include "mem/page.h"
#include "task.h"

#if defined(__x86_64__) || defined(__amd64__)
#include "smp_x64.h"
#endif

typedef struct cpu_local_info {
    tcb_t             current_task;
    tcb_t             idle_task;
    void             *sched_handle;
    uint32_t          id;
    page_directory_t *directory;
    bool              enable;
    arch_cpu_t        arch_data;
} cpu_local_t;

#if defined(__x86_64__) || defined(__amd64__)
bool x2apic_mode_supported();
#endif

cpu_local_t *get_cpu_local(size_t id);
_Noreturn void arch_ap_cpu_entry(); // 由架构具体实现
void smp_init();
