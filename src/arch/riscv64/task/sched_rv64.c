#include "io.h"
#include "mem/page.h"
#include "task/smp.h"
#include "task/task.h"
#include "timer.h"

extern void kernel_thread_func();                        // kthread.S
extern void fpu_save_context(fpu_context_t *fpu_ctx);    // fpu_context.S
extern void fpu_restore_context(fpu_context_t *fpu_ctx); // fpu_context.S

size_t sched_clock() {
    return nano_time();
}

void arch_send_scheduler() {
    //TODO
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    uint64_t stack_top = (uint64_t)((uint64_t)new_task + STACK_SIZE);
    memset(&new_task->context, 0, sizeof(struct arch_context_));
    memset(&new_task->context.fpu_ctx, 0, sizeof(fpu_context_t));
    new_task->context.fpu_ctx.fcsr = FCSR_INIT_DEFAULT;
    new_task->context.ctx          = (struct pt_regs *)stack_top - 1;
    new_task->context.ctx->sstatus =
        (2UL << 32) | (1UL << 18) | (1UL << 5) | (1UL << 0) | (1UL << 8) | (1UL << 1);
    new_task->context.ctx->s1  = new_task->_start;
    new_task->context.ctx->a2  = (uint64_t)args;
    new_task->context.ra       = (uint64_t)kernel_thread_func;
    new_task->context.ctx->epc = (uint64_t)kernel_thread_func;
    new_task->context.ctx->sp  = (uint64_t)new_task->context.ctx;
    new_task->context.sp       = (uint64_t)new_task->context.ctx;
    new_task->context.dead     = false;
    new_task->context.ctx->tp  = (uint64_t)arch_current_cpu();
    new_task->context.ctx->ktp = (uint64_t)arch_current_cpu();
}

void arch_context_init(tcb_t thread, struct arch_context_ *context) {
    uint64_t stack_top  = (uint64_t)((uint64_t)thread + STACK_SIZE);
    thread->context.ctx = (struct pt_regs *)stack_top - 1;
    context->ctx->sstatus =
        (2UL << 32) | (1UL << 18) | (3UL << 13) | (1UL << 5) | (1UL << 0) | (1UL << 8) | (1UL << 1);
    context->sp       = (uint64_t)context->ctx;
    context->ctx->s1  = 0; // entry
    context->ctx->a2  = 0; // initial_arg
    context->ctx->sp  = (uint64_t)context->ctx;
    context->ctx->tp  = (uint64_t)arch_current_cpu();
    context->ctx->ktp = (uint64_t)arch_current_cpu();
}

USED void __switch_to(tcb_t current, tcb_t next) { // switch_to call
    page_directory_t *dir = current->process->directory;
    if (dir != next->process->directory) { switch_page_directory(next->process->directory); }

    if (current->context.ctx->sstatus & (1UL << 63)) {
        if (SSTATUS_GET_FS(current->context.ctx->sstatus) == 3) {
            fpu_save_context(&current->context.fpu_ctx);
            SSTATUS_SET_FS(current->context.ctx->sstatus, 2);
        }
    }

    if (SSTATUS_GET_FS(next->context.ctx->sstatus) != 0) {
        fpu_restore_context(&next->context.fpu_ctx);
        SSTATUS_SET_FS(next->context.ctx->sstatus, 2);
    }
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    switch_to(current, next);
}

_Noreturn void arch_switch_to_user_mode() {
    while (true)
        arch_wait_for_interrupt();
}
