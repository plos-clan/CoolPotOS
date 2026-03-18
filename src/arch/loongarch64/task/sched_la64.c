#include "task/task.h"
#include "timer.h"

void arch_send_scheduler() {
    // TODO
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    // TODO
}

void arch_context_init(tcb_t thread, struct arch_context_ *context) {
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
}

_Noreturn void arch_switch_to_user_mode() {
}

void arch_context_free(tcb_t thread) {
    // TODO
}

bool arch_check_user_mode(const struct pt_regs *regs) {
    return true; //TODO
}
