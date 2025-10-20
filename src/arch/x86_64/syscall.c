#include "syscall.h"
#include "errno.h"
#include "fsgsbase.h"
#include "io.h"
#include "nr.h"
#include "task/task.h"
#include "term/klog.h"

__attribute__((naked)) void asm_syscall_handle() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "cld\n\t"
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov [rax+0x08], rsp\n\t"
                     "cmp QWORD PTR [rax+0x18], 0\n\t"
                     "je normal\n\t"
                     "signal:\n\t" //信号处理
                     "mov rsp, [rax+0x10]\n\t"
                     "jmp next\n\t"
                     "normal:\n\t" // 默认处理
                     "mov rsp, [rax+0x0] \n\t"
                     "jmp next\n\t"
                     "next:\n\t"
                     "sub rsp, 0x38\n\t"
                     "mov rax, cr2\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "push rbp\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push r8\n\t"
                     "push r9\n\t"
                     "push r10\n\t"
                     "push r11\n\t"
                     "push r12\n\t"
                     "push r13\n\t"
                     "push r14\n\t"
                     "push r15\n\t"
                     "mov rdi, rsp\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsi, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "call syscall_handler\n\t"
                     "pop r15\n\t"
                     "pop r14\n\t"
                     "pop r13\n\t"
                     "pop r12\n\t"
                     "pop r11\n\t"
                     "pop r10\n\t"
                     "pop r9\n\t"
                     "pop r8\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop rbp\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "add rsp, 0x38\n\t"
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsp, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "sysretq\n\t" ::
                         : "memory");
}

void arch_enable_syscall() {
    uint64_t efer;
    efer           = rdmsr(MSR_EFER);
    efer          |= 1;
    uint64_t star  = ((uint64_t)((0x18 | 0x3) - 8) << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_EFER, efer);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)asm_syscall_handle);
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT]  = (syscall_t)syscall_exit,
    [SYSCALL_OPEN]  = (syscall_t)syscall_open,
    [SYSCALL_CLOSE] = (syscall_t)syscall_close,
};

USED void syscall_handler(struct syscall_regs *regs, uint64_t user_regs) { // syscall 指令处理
    regs->rip    = regs->rcx;
    regs->rflags = regs->r11;
    regs->cs     = (0x20 | 0x3);
    regs->ss     = (0x18 | 0x3);
    regs->ds     = (0x18 | 0x3);
    regs->es     = (0x18 | 0x3);
    regs->rsp    = user_regs;

    tcb_t thread = get_current_task();
    write_fsbase((uint64_t)thread);
    thread->context.regs.rsp    = regs->rsp;
    thread->context.regs.rip    = regs->rip;
    thread->context.regs.rflags = regs->rflags;
    thread->context.regs.cs     = regs->cs;
    thread->context.regs.ss     = regs->ss;
    thread->context.regs.ds     = regs->ds;
    thread->context.regs.es     = regs->es;
    thread->context.regs.rdi    = regs->rdi;
    thread->context.regs.rsi    = regs->rsi;
    thread->context.regs.rdx    = regs->rdx;
    thread->context.regs.r10    = regs->r10;
    thread->context.regs.r8     = regs->r8;
    thread->context.regs.r9     = regs->r9;
    thread->context.regs.r15    = regs->r15;
    thread->context.regs.r14    = regs->r14;
    thread->context.regs.r13    = regs->r13;
    thread->context.regs.r12    = regs->r12;
    thread->context.regs.r11    = regs->r11;
    thread->context.regs.rbx    = regs->rbx;
    thread->context.regs.rcx    = regs->rcx;
    thread->context.regs.rbp    = regs->rbp;

    uint64_t syscall_id = regs->rax & 0xFFFFFFFF;
    if (likely(syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL)) {
        arch_open_interrupt();
        regs->rax = ((syscall_t)syscall_handlers[syscall_id])(regs->rdi, regs->rsi, regs->rdx,
                                                              regs->r10, regs->r8, regs->r9, regs);
        arch_close_interrupt();
    } else {
        if (unlikely(syscall_id != 12)) logkf("Syscall(%d) cannot implemented.\n", syscall_id);
        regs->rax = -ENOSYS;
    }

    write_fsbase(thread->context.fs_base);
}
