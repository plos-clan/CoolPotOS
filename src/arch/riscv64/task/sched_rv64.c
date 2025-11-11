#include "task/task.h"
#include "timer.h"

size_t sched_clock() {
    return nano_time();
}

void arch_send_scheduler() {
    //TODO
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    //TODO
}

void arch_context_init(struct arch_context_ *context) {
    //TODO
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    //TODO
}

_Noreturn void arch_switch_to_user_mode() {
    while (true) arch_wait_for_interrupt();
}
