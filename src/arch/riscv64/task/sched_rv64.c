#include "io.h"
#include "mem/page.h"
#include "task/smp.h"
#include "task/task.h"
#include "timer.h"

extern void kernel_thread_func();                        // kthread.S
extern void fpu_save_context(fpu_context_t *fpu_ctx);    // fpu_context.S
extern void fpu_restore_context(fpu_context_t *fpu_ctx); // fpu_context.S
extern void ret_from_trap_handler();                     // vector.s

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
    tcb_t                 thread    = get_current_task();
    uint64_t              stack_top = (uint64_t)((uint64_t)thread + STACK_SIZE);
    struct arch_context_ *context   = &thread->context;
    context->ctx                    = (struct pt_regs *)stack_top - 1;

    context->ra = (uint64_t)ret_from_trap_handler;
    context->sp = (uint64_t)context->ctx;

    context->ctx->ktp = (uint64_t)arch_current_cpu();
    context->ctx->tp  = (uint64_t)arch_current_cpu();

    uint64_t page_flags = ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_WRITE | ARCH_PT_FLAG_USER |
                          ARCH_PT_FLAG_EXEC | ARCH_PT_FLAG_READ;

    thread->context.user_stack =
        page_alloc_random(thread->process->directory, BIG_USER_STACK + PAGE_SIZE, page_flags);
    thread->context.user_stack_top =
        thread->context.user_stack + BIG_USER_STACK;

    vma_t *stack_vma = vma_alloc();

    stack_vma->vm_start  = thread->context.user_stack;
    stack_vma->vm_end    = thread->context.user_stack_top;
    stack_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

    stack_vma->vm_type = VMA_TYPE_ANON;
    stack_vma->vm_name = strdup("[stack]");

    vma_t *region =
        vma_find_intersection(&thread->process->vma_manager, thread->context.user_stack,
                              thread->context.user_stack_top);
    if (!region) {
        vma_insert(&thread->process->vma_manager, stack_vma);
    }

    context->ctx->epc     = thread->_start;
    context->ctx->sp      = thread->context.user_stack_top;
    context->ctx->sstatus = (2UL << 32) | (1UL << 18) | (3UL << 13) | (1UL << 5) | (1UL << 0);

    __asm__ volatile("mv sp, %0\n\t"
                     "j ret_from_trap_handler\n\t" ::"r"(context->ctx));

    while (true)
        arch_wait_for_interrupt();
}
