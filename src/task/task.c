#include "task/task.h"
#include "cow_arraylist.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "metadata.h"
#include "term/klog.h"

pcb_t                  kernel_process;
cow_arraylist         *process_list;
_Atomic volatile pid_t now_pid = 0;
_Atomic volatile pid_t now_tid = 0;

pid_t alloc_pid() {
    return now_pid++;
}

pid_t alloc_tid() {
    return now_tid++;
}

void setup_task() {
    process_list                  = cow_list_create();
    kernel_process                = calloc(1, sizeof(struct process_control_block));
    kernel_process->name          = strdup("System");
    kernel_process->pid           = alloc_pid();
    kernel_process->parent        = kernel_process;
    kernel_process->pl_index      = cow_list_add(process_list, kernel_process);
    kernel_process->cwd           = get_rootdir();
    kernel_process->child_threads = cow_list_create();

    tcb_t idle_thread     = malloc(STACK_SIZE);
    idle_thread->process  = kernel_process;
    idle_thread->tid      = alloc_tid();
    idle_thread->ct_index = cow_list_add(kernel_process->child_threads, idle_thread);
    kinfo("kernel process(%s) PID: %d ", kernel_process->name, kernel_process->pid);
}
