#include "pcb.h"
#include "krlibc.h"
#include "alloc.h"
#include "kprint.h"
#include "scheduler.h"
#include "io.h"

extern pcb_t current_task;
extern pcb_t kernel_head_task;

static int now_pid = 0;

int create_kernel_thread(int (*_start)(void *arg), void *args, char *name){
    pcb_t new_task = (pcb_t)malloc(STACK_SIZE);
    new_task->task_level = TASK_KERNEL_LEVEL;
    new_task->pid = now_pid++;
    new_task->cpu_clock = 0;
    new_task->tty = alloc_default_tty();
    new_task->directory = get_kernel_pagedir();
    new_task->context0.rflags = kernel_head_task->context0.rflags;
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top = (uint64_t *) ((uint64_t) new_task + STACK_SIZE);
    *(--stack_top) = (uint64_t) args;
    *(--stack_top) = (uint64_t) _start;
    new_task->context0.rsp = (uint64_t) new_task + STACK_SIZE - sizeof(uint64_t) * 3;//设置上下文

    add_task(new_task);
    return new_task->pid;
}

void init_pcb(){
    kernel_head_task = current_task = (pcb_t)malloc(STACK_SIZE);
    current_task->task_level = TASK_KERNEL_LEVEL;
    current_task->pid = now_pid++;
    current_task->cpu_clock = 0;
    current_task->directory = get_kernel_pagedir();
    current_task->kernel_stack = current_task->context0.rsp = (uint64_t)current_task + STACK_SIZE;
    current_task->context0.rflags = get_rflags();
    current_task->tty = get_default_tty();
    memcpy(current_task->name, "CP_IDLE\0", 9);
    current_task->next = current_task;
    kinfo("Load task schedule. | KernelProcessName: %s PID: %d", current_task->name, current_task->pid);
}
