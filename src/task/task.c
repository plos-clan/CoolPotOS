#include "task/task.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "metadata.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "term/klog.h"

pcb_t                  kernel_process;
tcb_t                  bsp_idle_thread;
cow_arraylist         *process_list;
_Atomic volatile pid_t now_pid = 0;
_Atomic volatile pid_t now_tid = 0;

pid_t alloc_pid() {
    return now_pid++;
}

pid_t alloc_tid() {
    return now_tid++;
}

tcb_t get_current_task() {
    return arch_current_cpu()->current_task;
}

pid_t create_process(const char *name, pcb_t parent, uint64_t flags) {
    pcb_t new_pgb = calloc(1, sizeof(struct process_control_block));
    if (new_pgb == NULL) return -ENOMEM;
    new_pgb->name          = strdup(name);
    new_pgb->pl_index      = cow_list_add(process_list, new_pgb);
    new_pgb->pid           = alloc_pid();
    new_pgb->parent        = parent == NULL ? kernel_process : parent;
    new_pgb->child_threads = cow_list_create();
    new_pgb->tty           = new_pgb->parent->tty;
    new_pgb->fdts          = fds_init();
    if (flags & CLONE_VM) {
        new_pgb->directory = clone_page_directory(new_pgb->parent->directory, false);
    } else
        new_pgb->directory = new_pgb->parent->directory;
    return new_pgb->pid;
}

pid_t create_kernel_thread(const char *name, int (*func)(void *arg), void *arg, pcb_t process,
                           uint64_t prio) {
    tcb_t thread = calloc(1, STACK_SIZE);
    not_null_assert(thread, "create kernel thread null.");
    thread->name     = strdup(name);
    thread->tid      = alloc_tid();
    thread->process  = process == NULL ? kernel_process : process;
    thread->ct_index = cow_list_add(thread->process->child_threads, thread);
    thread->prio     = prio;
    thread->_start   = (uint64_t)func;
    thread->status   = T_CREATE;
    arch_context_init_thread(thread, arg);
    add_task_prio(thread, thread->prio);
    return thread->tid;
}

void setup_task() {
    extern tty_t *kernel_session;
    process_list                  = cow_list_create();
    kernel_process                = calloc(1, sizeof(struct process_control_block));
    kernel_process->name          = strdup("System");
    kernel_process->pid           = alloc_pid();
    kernel_process->parent        = kernel_process;
    kernel_process->pl_index      = cow_list_add(process_list, kernel_process);
    kernel_process->cwd           = get_rootdir();
    kernel_process->child_threads = cow_list_create();
    kernel_process->directory     = get_kernel_pagedir();
    kernel_process->tty           = kernel_session;
    kernel_process->status        = T_RUNNING;
    kernel_process->exec          = NULL;
    kernel_process->fdts          = fds_init();

    bsp_idle_thread           = malloc(STACK_SIZE);
    bsp_idle_thread->process  = kernel_process;
    bsp_idle_thread->tid      = alloc_tid();
    bsp_idle_thread->ct_index = cow_list_add(kernel_process->child_threads, bsp_idle_thread);
    bsp_idle_thread->status   = T_RUNNING;
    arch_context_init(&bsp_idle_thread->context);
    kinfo("kernel process(%s) PID: %d ", kernel_process->name, kernel_process->pid);
}
