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

#define TICK_NSEC ((1000000000+SCHED_TIMER_SPEED/2)/SCHED_TIMER_SPEED)

tcb_t pick_next_task(uint64_t cpu_id);
bool add_task_prio(tcb_t thread, uint64_t prio);
bool add_task_prio_cpu(tcb_t thread, uint64_t prio,cpu_local_t *cpu);
void set_cpu_idle_task(tcb_t thread, cpu_local_t *cpu);
void set_bsp_cpu_info(cpu_local_t *bsp_cpu);
void remove_task(tcb_t thread,cpu_local_t *cpu);
void change_task_weight(tcb_t thread,uint64_t prio);
void scheduler_handler(uint64_t irq_num, void *data, struct pt_regs *regs);
void scheduler_nano_sleep(uint64_t nano);
void enable_scheduler();
void disable_scheduler();
void scheduler_yield();
