#include "signal.h"
#include "klog.h"
#include "pcb.h"

void check_pending_signals(pcb_t proc, tcb_t thread) {}

void register_signal(pcb_t task, int sig, void (*handler)(int)) {
    task->task_signal->signal_handlers[sig] = handler;
}

void setup_signal_thread(tcb_t thread, int signum, void *handler) {
    uint64_t new_stack = thread->context0.rsp;
    uint64_t old_rip   = thread->context0.rip;

    new_stack                -= 8;
    *((uint64_t *)new_stack)  = old_rip;
    thread->context0.rsp      = new_stack;
    thread->context0.rip      = (uint64_t)handler;

    if ((thread->context0.rsp & 0xF) != 8) { thread->context0.rsp -= 8; }
}

int send_signal(int pid, int sig) {
    if (sig < 0 || sig >= MAX_SIGNALS) return -EINVAL;

    pcb_t target = found_pcb(pid);
    if (!target) return -ESRCH;

    // SIGKILL 是不能被屏蔽的
    if (sig == SIGKILL) {
        kill_proc(target, 135);
        return 0;
    }

    logkf("Send interrupt signal: %d\n", sig);

    return 0;
}

int signal_action(int sig, sigaction_t *action, sigaction_t *oldaction) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL) { return -EINVAL; }

    sigaction_t *ptr = &get_current_task()->parent_group->task_signal->actions[sig];
    if (oldaction) { *oldaction = *ptr; }

    if (action) { *ptr = *action; }

    if (ptr->sa_flags & SIG_NOMASK) {
        ptr->sa_mask = 0;
    } else {
        ptr->sa_mask |= SIGMASK(sig);
    }

    return EOK;
}
