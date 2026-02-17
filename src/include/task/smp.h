#pragma once

#include "mem/page.h"
#include "task.h"

#if defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    include "smp_rv64.h"
#elif defined(__x86_64__) || defined(__amd64__)
#    include "smp_x64.h"
#elif defined(__loongarch__) || defined(__loongarch64)
#    include "smp_la64.h"
#endif

typedef struct cpu_local_info {
    tcb_t current_task;          // 当前任务
    tcb_t idle_task;             // IDLE任务
    void *sched_handle;          // 调度器句柄
    uint32_t id;                 // cpuid
    page_directory_t *directory; // 核心当前页表
    bool enable;                 // 该核心是否启用
    arch_cpu_t arch_data;        // 架构私有数据
    size_t task_count;           // 任务数量
    uint64_t jiffies;            // 时钟计数器计数 (不包含 yield)
    uint64_t idle_jiffies;       // 空闲时钟计数
    bool is_yield;               // 此次调度是否为 yield
} __attribute__((packed)) cpu_local_t;

#if defined(__x86_64__) || defined(__amd64__)
bool x2apic_mode_supported();
#endif

cpu_local_t *get_min_task_count_cpu();
cpu_local_t *get_cpu_local(size_t id);
uint64_t get_bsp_cpu_id();
size_t get_cpu_count();
cpu_local_t *arch_current_cpu(); // 由架构具体实现
void arch_bsp_cpu_init();        // 由架构具体实现
void smp_init();
