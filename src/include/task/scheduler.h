#pragma once

#include "task.h"
#include "eevdf.h"

tcb_t pick_next_task(uint64_t cpu_id);
bool add_task_prio(tcb_t thread, uint64_t prio);
bool add_task_prio_cpu(tcb_t thread, uint64_t prio,cpu_local_t *cpu);
void set_cpu_idle_task(tcb_t thread, cpu_local_t *cpu);
void remove_task(tcb_t thread,cpu_local_t *cpu);
void change_task_weight(tcb_t thread,uint64_t prio);
void scheduler_handler(uint64_t irq_num, void *data, struct pt_regs *regs);
void scheduler_nano_sleep(uint64_t nano);
void enable_scheduler();
void disable_scheduler();
void scheduler_yield();
