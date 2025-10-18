#include "intctl.h"
#include "ptrace.h"
#include "task/scheduler.h"

void arch_task_scheduler(struct pt_regs *regs){
    tcb_t thread = pick_next_task();
}
