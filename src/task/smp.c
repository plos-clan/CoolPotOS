#include "task/smp.h"
#include "krlibc.h"
#include "mem/buddy.h"
#include "term/klog.h"

volatile bool smp_enable = false;

uint64_t    cpu_count  = 0;
uint64_t    bsp_cpu_id = 0;
cpu_local_t cpu_local_infos[MAX_CPU];

extern void
smp_cpu_init(uint64_t *cpu_count, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos); // boot

cpu_local_t *get_cpu_local(size_t id) {
    if (id >= MAX_CPU)
        return NULL;
    for (size_t i = 0; i < cpu_count; i++) {
        if (!cpu_local_infos[i].enable)
            continue;
        if (cpu_local_infos[i].id == id)
            return &cpu_local_infos[i];
    }
    return NULL;
}

cpu_local_t *get_min_task_count_cpu() {
    cpu_local_t *local     = NULL;
    size_t       old_count = SIZE_MAX;
    for (size_t i = 0; i < cpu_count; i++) {
        if (!cpu_local_infos[i].enable)
            continue;
        if (cpu_local_infos[i].task_count < old_count) {
            old_count = cpu_local_infos[i].task_count;
            local     = &cpu_local_infos[i];
        }
    }
    return local;
}

uint64_t get_bsp_cpu_id() {
    return bsp_cpu_id;
}

size_t get_cpu_count() {
    return cpu_count;
}

void smp_init() {
    smp_cpu_init(&cpu_count, &bsp_cpu_id, cpu_local_infos);
    arch_bsp_cpu_init();
    smp_enable = true;
    kinfo("%d processors have been enabled.", cpu_count);
    percpu_pagecache_init();
    kinfo("buddy per-cpu page cache enable.");
}
