#include "thread.h"
#include "krlibc.h"
#include "user_malloc.h"

static void add_thread(pcb_t *pcb,cp_thread_t thread){ //添加线程至调度链
    if(pcb == NULL || thread == NULL) return;
    cp_thread_t tailt = pcb->thread_head;
    while (tailt->next != pcb->thread_head) {
        if(tailt->next == NULL) break;
        tailt = tailt->next;
    }
    tailt->next = thread;
}

void create_user_thread(pcb_t *pcb,void* func){
    cp_thread_t thread = kmalloc(STACK_SIZE);
    thread->next = NULL;
    thread->father_pcb = pcb;
    thread->tid = pcb->now_tid++;
    thread->kernel_stack = thread;
    thread->user_stack = pcb->user_stack;

    uint32_t *stack_top = (uint32_t * )((uint32_t) thread->kernel_stack + STACK_SIZE);
    *(--stack_top) = (uint32_t) func;
    *(--stack_top) = (uint32_t) process_exit;
    *(--stack_top) = (uint32_t) switch_to_user_mode;
    thread->context.esp = (uint32_t) thread + STACK_SIZE - sizeof(uint32_t) * 3;
    thread->context.eflags = 0x200;

    if(pcb->thread_head == NULL){
        pcb->thread_head = thread;
    }else add_thread(pcb,thread);
}