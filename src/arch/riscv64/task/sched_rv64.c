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
    context->ctx.sstatus = (2UL << 32) | (1UL << 18) | (3UL << 13) |
                            (1UL << 5) | (1UL << 0) | (1UL << 8);
    context->sp = (uint64_t)&context->ctx;
    context->ctx.s1 = 0; // entry
    context->ctx.a2 = 0; // initial_arg
    context->ctx.sp = (uint64_t)&context->ctx;
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    //TODO
}

_Noreturn void arch_switch_to_user_mode() {
    while (true) arch_wait_for_interrupt();
}
