#include "mem/page.h"
#include "task/task.h"
#include "timer.h"
#include "io.h"

extern void kernel_thread_func(); // kthread.S
extern void fpu_save_context(fpu_context_t *fpu_ctx); // fpu_context.S
extern void fpu_restore_context(fpu_context_t *fpu_ctx); // fpu_context.S

size_t sched_clock() {
    return nano_time();
}

void arch_send_scheduler() {
    //TODO
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    uint64_t *stack_top      = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    memset(&new_task->context, 0, sizeof(struct arch_context_));
    memset(&new_task->context.fpu_ctx, 0, sizeof(fpu_context_t));
    new_task->context.ctx.s1 = new_task->_start;
    new_task->context.ctx.a2 = (uint64_t)args;
    new_task->context.ra     = (uint64_t)kernel_thread_func;

}

void arch_context_init(struct arch_context_ *context) {
    context->ctx.sstatus =
        (2UL << 32) | (1UL << 18) | (3UL << 13) | (1UL << 5) | (1UL << 0) | (1UL << 8);
    context->sp     = (uint64_t)&context->ctx;
    context->ctx.s1 = 0; // entry
    context->ctx.a2 = 0; // initial_arg
    context->ctx.sp = (uint64_t)&context->ctx;
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    page_directory_t *dir = current->process->directory;
    if (dir != next->process->directory) { switch_page_directory(next->process->directory); }

    if (current->context.ctx.sstatus & (1UL << 63)) {
        if (SSTATUS_GET_FS(current->context.ctx.sstatus) == 3) {
            fpu_save_context(&current->context.fpu_ctx);
            SSTATUS_SET_FS(current->context.ctx.sstatus, 2);
        }
    }

    if (SSTATUS_GET_FS(next->context.ctx.sstatus) != 0) {
        fpu_restore_context(&next->context.fpu_ctx);
        SSTATUS_SET_FS(next->context.ctx.sstatus, 2);
    }

    current->context.ctx.ra = regs->ra;
    current->context.ctx.sp = regs->sp;
    current->context.ctx.gp = regs->gp;
    current->context.ctx.tp = regs->tp;
    current->context.ctx.t0 = regs->t0;
    current->context.ctx.t1 = regs->t1;
    current->context.ctx.t2 = regs->t2;
    current->context.ctx.s0 = regs->s0;
    current->context.ctx.s1 = regs->s1;
    current->context.ctx.a0 = regs->a0;
    current->context.ctx.a1 = regs->a1;
    current->context.ctx.a2 = regs->a2;
    current->context.ctx.a3 = regs->a3;
    current->context.ctx.a4 = regs->a4;
    current->context.ctx.a5 = regs->a5;
    current->context.ctx.a6 = regs->a6;
    current->context.ctx.a7 = regs->a7;
    current->context.ctx.s2 = regs->s2;
    current->context.ctx.s3 = regs->s3;
    current->context.ctx.s4 = regs->s4;
    current->context.ctx.s5 = regs->s5;
    current->context.ctx.s6 = regs->s6;
    current->context.ctx.s7 = regs->s7;
    current->context.ctx.s8 = regs->s8;
    current->context.ctx.s9 = regs->s9;
    current->context.ctx.s10 = regs->s10;
    current->context.ctx.s11 = regs->s11;
    current->context.ctx.t3 = regs->t3;
    current->context.ctx.t4 = regs->t4;
    current->context.ctx.t5 = regs->t5;
    current->context.ctx.t6 = regs->t6;
    current->context.ctx.epc = regs->epc;
    current->context.ctx.scause = regs->scause;
    current->context.ctx.stval = regs->stval;
    current->context.ctx.sstatus = regs->sstatus;

    regs->ra = next->context.ctx.ra;
    regs->sp = next->context.ctx.sp;
    regs->gp = next->context.ctx.gp;
    regs->tp = next->context.ctx.tp;
    regs->t0 = next->context.ctx.t0;
    regs->t1 = next->context.ctx.t1;
    regs->t2 = next->context.ctx.t2;
    regs->s0 = next->context.ctx.s0;
    regs->s1 = next->context.ctx.s1;
    regs->a0 = next->context.ctx.a0;
    regs->a1 = next->context.ctx.a1;
    regs->a2 = next->context.ctx.a2;
    regs->a3 = next->context.ctx.a3;
    regs->a4 = next->context.ctx.a4;
    regs->a5 = next->context.ctx.a5;
    regs->a6 = next->context.ctx.a6;
    regs->a7 = next->context.ctx.a7;
    regs->s2 = next->context.ctx.s2;
    regs->s3 = next->context.ctx.s3;
    regs->s4 = next->context.ctx.s4;
    regs->s5 = next->context.ctx.s5;
    regs->s6 = next->context.ctx.s6;
    regs->s7 = next->context.ctx.s7;
    regs->s8 = next->context.ctx.s8;
    regs->s9 = next->context.ctx.s9;
    regs->s10 = next->context.ctx.s10;
    regs->s11 = next->context.ctx.s11;
    regs->t3 = next->context.ctx.t3;
    regs->t4 = next->context.ctx.t4;
    regs->t5 = next->context.ctx.t5;
    regs->t6 = next->context.ctx.t6;
    regs->epc = next->context.ctx.epc;
    regs->scause = next->context.ctx.scause;
    regs->stval = next->context.ctx.stval;
    regs->sstatus = next->context.ctx.sstatus;
}

_Noreturn void arch_switch_to_user_mode() {
    while (true)
        arch_wait_for_interrupt();
}
