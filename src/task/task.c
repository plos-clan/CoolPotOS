#include "task/task.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "mem/lazy_alloc.h"
#include "metadata.h"
#include "task/futex.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "term/klog.h"

pcb_t                  kernel_process;
tcb_t                  bsp_idle_thread;
cow_arraylist         *process_list;
_Atomic volatile pid_t now_pid = 0;
_Atomic volatile pid_t now_tid = 0;
extern volatile bool   smp_enable;

pid_t alloc_pid() {
    return now_pid++;
}

pid_t alloc_tid() {
    return now_tid++;
}

tcb_t get_current_task() {
    return smp_enable ? arch_current_cpu()->current_task : NULL;
}

pcb_t found_pcb(pid_t pid) {
    pcb_t process = NULL;
    cow_foreach(process_list, process) {
        if (process->pid == pid) return process;
    }
    return NULL;
}



static void kill_thread0(pcb_t parent, tcb_t task) {
    task->status = T_OUT;
    free((void *)(task->syscall_stack - STACK_SIZE));
    free((void *)(task->signal_stack - STACK_SIZE));
    page_directory_t *src_dir = get_current_directory();
    switch_context_directory(task->process->directory);
    int *tid_addr = (int *)task->tid_address;
    if (tid_addr != NULL) *tid_addr = 0;
    switch_context_directory(src_dir);
}

static void kill_proc0(pcb_t pcb) {
    do {
        if(pcb->child_threads->size == 0) break;
        tcb_t thread = NULL;
        cow_foreach(pcb->child_threads, thread) {
            break;
        }
        if (thread == NULL || thread->status == T_OUT) break;
        cow_list_remove(pcb->child_threads, thread->ct_index);
        kill_thread0(pcb, thread);
        free(thread);
    } while (true);

    cow_list_destroy(pcb->child_threads);
    cow_list_remove(process_list, pcb->pl_index);

    //procfs_on_exit_task(pcb);

    for (size_t i = 0; i < pcb->fdts->fds_length; i++) {
        fd_t *handle = pcb->fdts->fds[i];
        if (handle != NULL) {
            vfs_close(handle->node);
            free(handle);
        }
    }

    lazy_free(pcb);

    free_fdt(pcb->fdts);
    ipc_queue_release(pcb->ipc_queue);
    free_llist_queue(pcb->virt_queue,NULL,NULL);
    free(pcb->cmdline);
    vfs_close(pcb->cwd);
    vfs_close(pcb->exec);
    if (pcb->envp) free_envp(pcb->envp);
    logkf("task: Freeing process %s (PID: %d) vfork: %s\n", pcb->name, pcb->pid,
          pcb->vfork ? "true" : "false");
    if (!pcb->vfork) free_page_directory(pcb->directory);
    free(pcb);
}

void kill_thread(tcb_t task) {
    if (task == NULL) return;
    task->status = T_DEATH;
    if (task->tid_directory != NULL) {
        page_directory_t *directory = get_current_directory();
        switch_context_directory(task->tid_directory);
        futex_wake((void *)arch_virt_to_phys(task->tid_address), 1);
        switch_context_directory(directory);
    }
    futex_free(task);
    remove_task(task, get_cpu_local(task->cpu_id));
}

void kill_proc(pcb_t pcb, int exit_code, bool is_zombie) {
    if (pcb == NULL) return;
    if (pcb->pid == kernel_process->pid) {
        kerror("Cannot kill System process.");
        return;
    }
    if (pcb->tty->fgproc == pcb->pid) { pcb->tty->fgproc = 0; }

    if (is_zombie) {
        disable_scheduler();
        if (pcb->child_threads->size > 0) {
            tcb_t tcb = NULL;
            cow_foreach(pcb->child_threads, tcb) {
                kill_thread(tcb);
            }
        }

        pcb->status       = T_ZOMBIE;
        ipc_message_t msg = malloc(sizeof(struct ipc_message));
        msg->pid          = pcb->pid;
        msg->type         = IPC_MSG_TYPE_EPID;
        msg->data[0]      = exit_code & 0xFF;
        msg->data[1]      = (exit_code >> 8) & 0xFF;
        msg->data[2]      = (exit_code >> 16) & 0xFF;
        msg->data[3]      = (exit_code >> 24) & 0xFF;
        ipc_send(pcb->parent->ipc_queue, msg);
        enable_scheduler();
    } else {
        cow_list_remove(pcb->parent->child_process, pcb->ppl_index);
        pcb->status = T_DEATH;
        kill_proc0(pcb);
    }
}

int waitpid(pid_t pid, pid_t *pid_ret) {
    get_current_task()->status = T_WAIT;
    bool is_sti                = arch_check_interrupt();
    arch_open_interrupt();

    pcb_t prcoess = get_current_task()->process;

    ipc_message_t mesg;
    int           exit_code;
    while (1) {
        change_task_weight(get_current_task(), NICE_TO_PRIO(10));
        mesg = ipc_recv_wait(prcoess->ipc_queue,IPC_MSG_TYPE_EPID);
        change_task_weight(get_current_task(), NICE_TO_PRIO(0));
        exit_code =
            (mesg->data[3] << 24) | (mesg->data[2] << 16) | (mesg->data[1] << 8) | mesg->data[0];
        if (pid == -1 || pid == mesg->pid) break;
        ipc_send(prcoess->ipc_queue, mesg);
    }
    pcb_t wait_p = found_pcb(mesg->pid);
    if (wait_p->status == T_ZOMBIE) kill_proc(wait_p, exit_code, false);
    *pid_ret = mesg->pid;
    free(mesg);

    if (!is_sti) arch_close_interrupt();
    get_current_task()->status = T_RUNNING;
    return exit_code;
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
    new_pgb->ipc_queue     = ipc_queue_init();
    new_pgb->virt_queue    = create_llist_queue();
    new_pgb->cwd           = get_rootdir();
    new_pgb->child_process = cow_list_create();
    new_pgb->ppl_index     = cow_list_add(new_pgb->parent->child_process, new_pgb);
    new_pgb->vfork         = false;
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
    thread->name          = strdup(name);
    thread->tid           = alloc_tid();
    thread->process       = process == NULL ? kernel_process : process;
    thread->ct_index      = cow_list_add(thread->process->child_threads, thread);
    thread->prio          = prio;
    thread->_start        = (uint64_t)func;
    thread->status        = T_CREATE;
    thread->signal_stack  = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    thread->syscall_stack = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
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
    kernel_process->virt_queue    = create_llist_queue();
    kernel_process->fdts          = fds_init();
    kernel_process->child_process = cow_list_create();
    kernel_process->ipc_queue     = ipc_queue_init();
    kernel_process->vfork         = false;

    bsp_idle_thread                = malloc(STACK_SIZE);
    bsp_idle_thread->process       = kernel_process;
    bsp_idle_thread->tid           = alloc_tid();
    bsp_idle_thread->ct_index      = cow_list_add(kernel_process->child_threads, bsp_idle_thread);
    bsp_idle_thread->status        = T_RUNNING;
    bsp_idle_thread->signal_stack  = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    bsp_idle_thread->syscall_stack = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    arch_context_init(&bsp_idle_thread->context);
    kinfo("kernel process(%s) PID: %d ", kernel_process->name, kernel_process->pid);
}
