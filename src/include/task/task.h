#pragma once

#include "types.h"
#include "ptrace.h"

typedef struct process_control_block *pcb_t;
typedef struct thread_control_block  *tcb_t;

struct process_control_block {
    pid_t pid;
    pcb_t parent;
};

struct thread_control_block {
    pid_t    tid;
    pcb_t    process;
    uint64_t prio;
};

void arch_task_scheduler(struct pt_regs *regs);
void setup_task();
