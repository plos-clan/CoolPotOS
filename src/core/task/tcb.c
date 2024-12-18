#include "tcb.h"
#include "krlibc.h"
#include "user_malloc.h"
#include "scheduler.h"
#include "io.h"

static void add_thread(pcb_t *pcb, tcb_t thread) { //添加线程至调度链
    if (pcb == NULL || thread == NULL) return;
    tcb_t tailt = pcb->main_thread_head;
    while (tailt->next != pcb->main_thread_head) {
        if (tailt->next == NULL) break;
        tailt = tailt->next;
    }
    tailt->next = thread;
}

void kill_user_thread(pcb_t *pcb, tcb_t thread) {
    disable_scheduler();
    pcb->sche_time--;
    tcb_t head = pcb->main_thread_head;
    tcb_t last = NULL;
    while (1) {
        if (head->tid == thread->tid) {
            last->next = thread->next;
            kfree(pcb);
            enable_scheduler();
            io_sti();
            return;
        }
        last = head;
        head = head->next;
    }
}

tcb_t create_kernel_subthread(pcb_t *pcb, void *func) {
    tcb_t thread = kmalloc(STACK_SIZE);
    thread->next = NULL;
    thread->father_pcb = pcb;
    thread->tid = pcb->now_tid++;
    thread->kernel_stack = thread;
    thread->user_stack = pcb->user_stack;
    pcb->sche_time++;

    uint32_t *stack_top = (uint32_t *) ((uint32_t) thread->kernel_stack + STACK_SIZE);
    *(--stack_top) = (uint32_t) process_exit;
    *(--stack_top) = (uint32_t) func;
    thread->context.esp = (uint32_t) thread + STACK_SIZE - sizeof(uint32_t) * 3;
    thread->context.eflags = 0x200;

    if (pcb->main_thread_head == NULL) {
        pcb->current_thread = pcb->main_thread_head = thread;
    } else add_thread(pcb, thread);

    return thread;
}

tcb_t create_user_thread(pcb_t *pcb, void *func) {
    tcb_t thread = kmalloc(STACK_SIZE);
    thread->next = NULL;
    thread->father_pcb = pcb;
    thread->tid = pcb->now_tid++;
    thread->kernel_stack = thread;
    thread->user_stack = pcb->user_stack;

    uint32_t *stack_top = (uint32_t *) ((uint32_t) thread->kernel_stack + STACK_SIZE);
    *(--stack_top) = (uint32_t) func;
    *(--stack_top) = (uint32_t) process_exit;
    *(--stack_top) = (uint32_t) switch_to_user_mode;
    thread->context.esp = (uint32_t) thread + STACK_SIZE - sizeof(uint32_t) * 3;
    thread->context.eflags = 0x200;

    if (pcb->main_thread_head == NULL) {
        pcb->current_thread = pcb->main_thread_head = thread;
    } else add_thread(pcb, thread);

    return thread;
}