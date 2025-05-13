#include "signal.h"
#include "pcb.h"

void check_pending_signals(pcb_t proc, tcb_t thread) {
    for (size_t sig = 0; sig < MAX_SIGNALS; sig++) {
        if ((proc->task_signal.pending_signals & (1 << sig)) &&
            !proc->task_signal.signal_mask[sig]) {
            proc->task_signal.pending_signals &= ~(1 << sig); // 清除信号

            if (proc->task_signal.signal_handlers[sig]) {
                setup_signal_thread(thread, sig, proc->task_signal.signal_handlers[sig]);
            } else {
                //TODO 执行默认行为
            }
        }
    }
}

void register_signal(pcb_t task, int sig, void (*handler)(int)) {
    signal_block_t block       = task->task_signal;
    block.signal_handlers[sig] = handler;
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

    if (target->task_signal.signal_mask[sig]) { target->task_signal.pending_signals |= (1 << sig); }

    return 0;
}
