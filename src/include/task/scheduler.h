#pragma once

#define MAX(x, y) ((x > y) ? (x) : (y))

#define MAX_NICE   19
#define MIN_NICE   -20
#define NICE_WIDTH (MAX_NICE - MIN_NICE + 1)

#define MAX_RT_PRIO 100
#define MAX_DL_PRIO 0

#define MAX_PRIO     (MAX_RT_PRIO + NICE_WIDTH)
#define DEFAULT_PRIO (MAX_RT_PRIO + NICE_WIDTH / 2)

#define NICE_TO_PRIO(nice) ((nice) + DEFAULT_PRIO)
#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIO)

#include "task.h"
#include "smp.h"

#define TICK_NSEC ((1000000000 + SCHED_TIMER_SPEED / 2) / SCHED_TIMER_SPEED)

bool scheduler_check_status();
tcb_t scheduler_pick_next(uint64_t cpu_id);
bool scheduler_add_task(tcb_t thread, uint64_t prio);
bool scheduler_add_task_cpu(tcb_t thread, uint64_t prio, cpu_local_t *cpu);
void scheduler_set_cpu_idle(tcb_t thread, cpu_local_t *cpu);
void scheduler_set_bsp_cpu(cpu_local_t *bsp_cpu);
void scheduler_remove_task(tcb_t thread, cpu_local_t *cpu);
void scheduler_change_weight(tcb_t thread, uint64_t prio);
void scheduler_handler(uint64_t irq_num, void *data, struct pt_regs *regs);
int scheduler_nano_sleep(uint64_t nano);
int scheduler_block_current(uint64_t timeout_ns, const char *reason);
void scheduler_unblock(tcb_t thread, int code);
void scheduler_check_sleep();
void scheduler_enable();
void scheduler_disable();
void scheduler_yield();
