#pragma once

#include "types.h"

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
