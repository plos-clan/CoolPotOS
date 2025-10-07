#pragma once

#define MAX_NICE   19
#define MIN_NICE   -20
#define NICE_WIDTH (MAX_NICE - MIN_NICE + 1)

#define MAX_RT_PRIO 100
#define MAX_DL_PRIO 0

#define MAX_PRIO     (MAX_RT_PRIO + NICE_WIDTH)
#define DEFAULT_PRIO (MAX_RT_PRIO + NICE_WIDTH / 2)

#define NICE_TO_PRIO(nice) ((nice) + DEFAULT_PRIO)
#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIO)

#define IDLE_PRIORITY NICE_TO_PRIO(20)
#define NORMAL_PRIORITY NICE_TO_PRIO(0)
#define KTHREAD_PRIORITY NICE_TO_PRIO(-5)

#include "cp_kernel.h"

pid_t    task_create(const char *name, void (*entry)(uint64_t), uint64_t arg, uint64_t priority);
uint64_t task_exit(int64_t code);
void     scheduler_yield();
