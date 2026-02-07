#include "task/signal_arch.h"

bool arch_signal_setup(tcb_t task, int signum, sigaction_t *action, struct syscall_regs *regs) {
    //TODO
    return false;
}

uint64_t arch_signal_sigreturn(struct syscall_regs *regs) {
    //TODO
    return  0;
}
