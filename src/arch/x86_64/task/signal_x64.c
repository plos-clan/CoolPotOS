#include "task/signal_arch.h"
#include "task/signal.h"
#include "krlibc.h"
#include "term/klog.h"

bool arch_signal_setup(tcb_t task, int signum, sigaction_t *action, struct syscall_regs *regs) {
    const uint64_t handler = (uint64_t)action->sa_handler;
    if (handler == 0 || handler == 1) {
        return false;
    }

    // Compute frame location on user stack (16-byte aligned)
    uint64_t user_rsp = task->syscall_stack_user;
    user_rsp -= sizeof(struct signal_frame);
    user_rsp &= ~0xFULL; // 16-byte align

    struct signal_frame *frame = (struct signal_frame *)user_rsp;

    // Save all registers from the syscall regs
    frame->r15    = regs->r15;
    frame->r14    = regs->r14;
    frame->r13    = regs->r13;
    frame->r12    = regs->r12;
    frame->r11    = regs->r11;
    frame->r10    = regs->r10;
    frame->r9     = regs->r9;
    frame->r8     = regs->r8;
    frame->rbx    = regs->rbx;
    frame->rcx    = regs->rcx;
    frame->rdx    = regs->rdx;
    frame->rsi    = regs->rsi;
    frame->rdi    = regs->rdi;
    frame->rbp    = regs->rbp;
    frame->rax    = regs->rax;
    frame->rip    = regs->rcx;                // rcx holds original RIP (from sysret convention)
    frame->rflags = regs->r11;                // r11 holds original RFLAGS
    frame->rsp    = task->syscall_stack_user; // original user RSP

    // Save signal mask and call_in_signal state
    if (task->has_saved_sigmask) {
        frame->saved_blocked    = task->saved_sigmask;
        task->has_saved_sigmask = false;
    } else {
        frame->saved_blocked = task->blocked;
    }
    frame->saved_call_in_signal = task->call_in_signal;
    frame->signum               = (uint32_t)signum;
    frame->_pad0                = 0;

    // Save FPU state
    save_fpu_context(&frame->fpu_state);

    // Set magic for validation
    frame->magic = SIGNAL_FRAME_MAGIC;

    // Build trampoline: mov rax, 15; syscall; nop...
    // 48 c7 c0 0f 00 00 00    mov rax, 15
    // 0f 05                    syscall
    frame->trampoline[0] = 0x48;
    frame->trampoline[1] = 0xc7;
    frame->trampoline[2] = 0xc0;
    frame->trampoline[3] = 0x0f; // 15 = SYSCALL_SIGRET
    frame->trampoline[4] = 0x00;
    frame->trampoline[5] = 0x00;
    frame->trampoline[6] = 0x00;
    frame->trampoline[7] = 0x0f;
    frame->trampoline[8] = 0x05;
    memset(&frame->trampoline[9], 0x90, 7); // nop padding

    // Set pretcode (return address for the handler's ret instruction)
    if (action->sa_flags & SA_RESTORER) {
        frame->pretcode = (uint64_t)action->sa_restorer;
    } else {
        frame->pretcode = (uint64_t)&frame->trampoline[0];
    }

    // Modify regs so sysretq jumps to the signal handler
    // sysretq: RIP = RCX, RFLAGS = R11
    regs->rcx = handler;
    regs->r11 = 0x202;            // IF set
    regs->rdi = (uint64_t)signum; // first argument to handler

    // Set user RSP to the signal frame (asm return path restores RSP from tcb->syscall_stack_user)
    task->syscall_stack_user = user_rsp;

    // Mark that we're in a signal handler
    task->call_in_signal = 1;

    // Update signal mask: block the signal being delivered (unless SA_NODEFER)
    if (!(action->sa_flags & SA_NODEFER)) {
        task->blocked |= SIGMASK(signum);
    }
    task->blocked |= action->sa_mask;
    // Never block SIGKILL or SIGSTOP
    task->blocked &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));

    // Clear pending bit
    task->signal &= ~SIGMASK(signum);

    // If SA_RESETHAND, reset handler to SIG_DFL
    if (action->sa_flags & SA_RESETHAND) {
        action->sa_handler = SIG_DFL;
    }

    return true;
}

uint64_t arch_signal_sigreturn(struct syscall_regs *regs) {
    tcb_t task = get_current_task();

    // The handler did 'ret', which popped pretcode.
    // Then trampoline did 'syscall'. On syscall entry, user RSP was saved
    // to tcb->syscall_stack_user. At that point RSP = &frame->signum.
    // So frame = syscall_stack_user - offsetof(signal_frame, signum)
    // = syscall_stack_user - 8 (pretcode is 8 bytes)
    struct signal_frame *frame =
        (struct signal_frame *)(task->syscall_stack_user
                                - __builtin_offsetof(struct signal_frame, signum));

    // Validate magic
    if (frame->magic != SIGNAL_FRAME_MAGIC) {
        logkf("sigreturn: bad magic %lx\n", frame->magic);
        return (uint64_t)-1;
    }

    // Restore registers
    regs->r15 = frame->r15;
    regs->r14 = frame->r14;
    regs->r13 = frame->r13;
    regs->r12 = frame->r12;
    regs->r11 = frame->rflags; // sysretq loads RFLAGS from R11
    regs->r10 = frame->r10;
    regs->r9  = frame->r9;
    regs->r8  = frame->r8;
    regs->rbx = frame->rbx;
    regs->rcx = frame->rip; // sysretq loads RIP from RCX
    regs->rdx = frame->rdx;
    regs->rsi = frame->rsi;
    regs->rdi = frame->rdi;
    regs->rbp = frame->rbp;

    // Restore user RSP
    task->syscall_stack_user = frame->rsp;

    // Restore signal mask and call_in_signal
    task->blocked        = frame->saved_blocked;
    task->call_in_signal = frame->saved_call_in_signal;

    // Restore FPU state
    restore_fpu_context(&frame->fpu_state);

    // Return the original rax (syscall return value before signal delivery)
    return frame->rax;
}
