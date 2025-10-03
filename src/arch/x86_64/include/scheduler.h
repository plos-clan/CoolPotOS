#pragma once

#define SCHED_NORMAL   0
#define SCHED_FIFO     1
#define SCHED_RR       2
#define SCHED_BATCH    3
#define SCHED_IDLE     5
#define SCHED_DEADLINE 6

#include "isr.h"

typedef struct registers {
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;   // 保留
    uint64_t err_code; // 保留
    // CPU自动压入
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t ss, cs, ds, es;
} TaskContext;

/**
 * 启用调度器 (所有CPU核心)
 */
void enable_scheduler();

/**
 * 关闭调度器 (所有CPU核心)
 */
void disable_scheduler();

/**
 * 调度器优化性休眠
 * @param nano
 */
void scheduler_nano_sleep(uint64_t nano);

/**
 * 主动触发调度器, 让出当前CPU核心的时间片
 */
void scheduler_yield();

void scheduler(registers_t *registers);
int  get_all_task();
