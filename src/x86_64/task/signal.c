#include "signal.h"
#include "pcb.h"

void check_pending_signals(pcb_t proc, tcb_t thread) {
    for (size_t sig = 0; sig < MAX_SIGNALS; sig++) {
        if ((proc->task_signal.pending_signals & (1 << sig)) &&
            !proc->task_signal.signal_mask[sig]) {
            proc->task_signal.pending_signals &= ~(1 << sig); // 清除信号

            if (proc->task_signal.signal_handlers[sig]) {
                setup_signal_stack(
                    thread, sig,
                    proc->task_signal.signal_handlers[sig]); // 构造用户态信号处理器栈帧
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

void setup_signal_stack(tcb_t thread, int signum, void *handler) {
    void *user_sp  = (void *)thread->context0.rsp;
    user_sp        = (void *)((uintptr_t)user_sp & ~0xF); // 对齐
    user_sp       -= sizeof(struct sigframe);

    /* 填充信号帧 */
    struct sigframe *frame = (struct sigframe *)user_sp;
    frame->sig             = signum;

    frame->context0.r15    = thread->context0.r15;
    frame->context0.r14    = thread->context0.r14;
    frame->context0.r13    = thread->context0.r13;
    frame->context0.r12    = thread->context0.r12;
    frame->context0.r11    = thread->context0.r11;
    frame->context0.r10    = thread->context0.r10;
    frame->context0.r9     = thread->context0.r9;
    frame->context0.r8     = thread->context0.r8;
    frame->context0.rax    = thread->context0.rax;
    frame->context0.rbx    = thread->context0.rbx;
    frame->context0.rcx    = thread->context0.rcx;
    frame->context0.rdx    = thread->context0.rdx;
    frame->context0.rdi    = thread->context0.rdi;
    frame->context0.rsi    = thread->context0.rsi;
    frame->context0.rbp    = thread->context0.rbp;
    frame->context0.rflags = thread->context0.rflags;
    frame->context0.rip    = thread->context0.rip;
    frame->context0.rsp    = thread->context0.rsp;
    frame->context0.ss     = thread->context0.ss;
    frame->context0.es     = thread->context0.es;
    frame->context0.cs     = thread->context0.cs;
    frame->context0.ds     = thread->context0.ds;

    /* 修改返回地址和参数 */
    thread->context0.rsp = (uint64_t)user_sp;
    thread->context0.rip = (uint64_t)handler;
    thread->context0.rdi = signum;
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
