#include "task/task.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "fs/procfs.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "mem/frame.h"
#include "mem/lazy_alloc.h"
#include "metadata.h"
#include "task/eevdf.h"
#include "task/futex.h"
#include "task/scheduler.h"
#include "task/signal.h"
#include "task/smp.h"
#include "term/klog.h"

static pcb_t kernel_process;
static tcb_t bsp_idle_thread;
static cow_arraylist *process_list;
_Atomic volatile pid_t now_pid = 0;
_Atomic volatile pid_t now_tid = 0;
extern volatile bool smp_enable;

typedef struct retired_thread_node {
    tcb_t thread;
    uint64_t epoch[MAX_CPU];
    struct retired_thread_node *next;
} retired_thread_node_t;

typedef struct retired_process_node {
    pcb_t process;
    uint64_t epoch[MAX_CPU];
    struct retired_process_node *next;
} retired_process_node_t;

static retired_thread_node_t *retired_threads    = NULL;
static retired_process_node_t *retired_processes = NULL;
static spin_t retired_lock                       = SPIN_INIT;
static spin_t task_exit_lock                     = SPIN_INIT;

mm_t *mm_create(page_directory_t *directory) {
    mm_t *mm = calloc(1, sizeof(mm_t));
    if (mm == NULL) {
        return NULL;
    }

    mm->directory = directory;
    __atomic_store_n(&mm->ref_count, 1, __ATOMIC_RELAXED);
    return mm;
}

mm_t *mm_clone(const mm_t *src) {
    if (src == NULL || src->directory == NULL) {
        return NULL;
    }

    page_directory_t *directory = clone_page_directory(src->directory, false);
    if (directory == NULL) {
        return NULL;
    }

    mm_t *mm = mm_create(directory);
    if (mm == NULL) {
        free_page_directory(directory);
        return NULL;
    }

    if (!vma_manager_clone((vma_manager_t *)&src->vma_manager, &mm->vma_manager)) {
        free_page_directory(directory);
        free(mm);
        return NULL;
    }

    return mm;
}

void mm_retain(mm_t *mm) {
    if (mm == NULL) {
        return;
    }

    __atomic_add_fetch(&mm->ref_count, 1, __ATOMIC_ACQ_REL);
}

void mm_release(mm_t *mm) {
    if (mm == NULL) {
        return;
    }

    if (__atomic_sub_fetch(&mm->ref_count, 1, __ATOMIC_ACQ_REL) != 0) {
        return;
    }

    vma_manager_exit_cleanup(&mm->vma_manager);
    if (mm->directory != NULL && mm->directory != get_kernel_pagedir()) {
        free_page_directory(mm->directory);
    }
    free(mm);
}

cow_arraylist *get_process_list() {
    return process_list;
}

pcb_t get_kernel_process() {
    return kernel_process;
}

tcb_t get_bsp_idle_thread() {
    return bsp_idle_thread;
}

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
        if (process->pid == pid) {
            return process;
        }
    }
    return NULL;
}

static void refresh_child_thread_links(const pcb_t process) {
    if (process == NULL || process->child_threads == NULL) {
        return;
    }

    for (size_t i = 0; i < process->child_threads->size; i++) {
        tcb_t thread = cow_list_get(process->child_threads, i);
        if (thread == NULL) {
            continue;
        }
        thread->ct_index = i;
    }
}

static void refresh_child_process_links(const pcb_t parent) {
    if (parent == NULL || parent->child_process == NULL) {
        return;
    }

    for (size_t i = 0; i < parent->child_process->size; i++) {
        pcb_t child = cow_list_get(parent->child_process, i);
        if (child == NULL) {
            continue;
        }
        child->parent    = parent;
        child->ppl_index = i;
    }
}

static void capture_reclaim_epoch(uint64_t epoch[MAX_CPU]) {
    memset(epoch, 0, sizeof(uint64_t) * MAX_CPU);
    for (size_t i = 0; i < get_cpu_count(); i++) {
        cpu_local_t *cpu = get_cpu_local_by_index(i);
        if (cpu != NULL && cpu->enable) {
            epoch[i] = cpu->jiffies;
        }
    }
}

static bool reclaim_epoch_elapsed(const uint64_t epoch[MAX_CPU]) {
    for (size_t i = 0; i < get_cpu_count(); i++) {
        cpu_local_t *cpu = get_cpu_local_by_index(i);
        if (cpu == NULL || !cpu->enable) {
            continue;
        }
        if (cpu->jiffies <= epoch[i]) {
            return false;
        }
    }
    return true;
}

static bool thread_reclaim_ready(tcb_t thread, const uint64_t epoch[MAX_CPU]) {
    if (thread == NULL) {
        return false;
    }

    cpu_local_t *cpu = get_cpu_local(thread->cpu_id);
    if (cpu != NULL && cpu->current_task == thread) {
        return false;
    }

    return reclaim_epoch_elapsed(epoch);
}

static void enqueue_retired_thread(tcb_t thread) {
    if (thread == NULL) {
        return;
    }

    retired_thread_node_t *node = malloc(sizeof(retired_thread_node_t));
    asserts(node, "enqueue_retired_thread: node is null.");
    node->thread = thread;
    node->next   = NULL;
    capture_reclaim_epoch(node->epoch);

    if (thread->process != NULL) {
        __atomic_add_fetch(&thread->process->retired_threads_pending, 1, __ATOMIC_ACQ_REL);
    }

    spin_lock(retired_lock);
    node->next      = retired_threads;
    retired_threads = node;
    spin_unlock(retired_lock);
}

static void enqueue_retired_process(pcb_t process) {
    if (process == NULL) {
        return;
    }

    retired_process_node_t *node = malloc(sizeof(retired_process_node_t));
    asserts(node, "enqueue_retired_process: node is null.");
    node->process = process;
    node->next    = NULL;
    capture_reclaim_epoch(node->epoch);

    spin_lock(retired_lock);
    node->next        = retired_processes;
    retired_processes = node;
    spin_unlock(retired_lock);
}

static void destroy_thread(tcb_t thread) {
    if (thread == NULL) {
        return;
    }

    if (thread->syscall_stack != 0) {
        free_frames(
            virt_to_phys((void *)(thread->syscall_stack - MAX_STACK_SIZE)),
            MAX_STACK_SIZE / PAGE_SIZE
        );
        thread->syscall_stack = 0;
    }
    if (thread->signal_stack != 0) {
        free_frames(
            virt_to_phys((void *)(thread->signal_stack - MAX_STACK_SIZE)),
            MAX_STACK_SIZE / PAGE_SIZE
        );
        thread->signal_stack = 0;
    }
    if (thread->name != NULL) {
        free(thread->name);
        thread->name = NULL;
    }

    arch_context_free(thread);
    free(thread);
}

static bool process_threads_reclaimed(const pcb_t pcb) {
    if (pcb == NULL || pcb->child_threads == NULL) {
        return false;
    }

    if (__atomic_load_n(&pcb->retired_threads_pending, __ATOMIC_ACQUIRE) != 0) {
        return false;
    }

    return cow_list_size(pcb->child_threads) == 0;
}

static bool claim_process_fd_release(const pcb_t pcb) {
    if (pcb == NULL) {
        return false;
    }

    spin_lock(task_exit_lock);
    if (pcb->exit_fds_released) {
        spin_unlock(task_exit_lock);
        return false;
    }
    pcb->exit_fds_released = true;
    spin_unlock(task_exit_lock);
    return true;
}

static void process_close_fds(const pcb_t pcb) {
    if (pcb == NULL || pcb->fdts == NULL) {
        return;
    }

    for (size_t i = 0; i < pcb->fdts->fds_length; i++) {
        fd_t *handle = pcb->fdts->fds[i];
        if (handle == NULL) {
            continue;
        }

        pcb->fdts->fds[i] = NULL;
        vfs_close(handle->node);
        free(handle);
    }

    free_fdt(pcb->fdts);
    pcb->fdts = NULL;

    if (pcb->exec) {
        vfs_close(pcb->exec);
        pcb->exec = NULL;
    }
}

static void release_process_fds_if_ready(const pcb_t pcb) {
    if (!process_threads_reclaimed(pcb)) {
        return;
    }

    if (!claim_process_fd_release(pcb)) {
        return;
    }

    process_close_fds(pcb);
}

static void destroy_process(pcb_t pcb) {
    if (pcb == NULL) {
        return;
    }

    process_close_fds(pcb);

    if (pcb->child_threads != NULL) {
        cow_list_destroy(pcb->child_threads);
        pcb->child_threads = NULL;
    }
    if (pcb->child_process != NULL) {
        cow_list_destroy(pcb->child_process);
        pcb->child_process = NULL;
    }

    if (pcb->name != NULL) {
        free(pcb->name);
        pcb->name = NULL;
    }
    if (pcb->mm != NULL) {
        mm_release(pcb->mm);
        pcb->mm = NULL;
    }

    free(pcb);
}

void task_reap_retired() {
    while (true) {
        retired_thread_node_t *prev = NULL;
        retired_thread_node_t *node = NULL;

        spin_lock(retired_lock);
        for (retired_thread_node_t *it = retired_threads; it != NULL; it = it->next) {
            if (thread_reclaim_ready(it->thread, it->epoch)) {
                node = it;
                break;
            }
            prev = it;
        }

        if (node != NULL) {
            if (prev != NULL) {
                prev->next = node->next;
            } else {
                retired_threads = node->next;
            }
        }
        spin_unlock(retired_lock);

        if (node == NULL) {
            break;
        }

        pcb_t owner = node->thread->process;
        destroy_thread(node->thread);
        if (owner != NULL) {
            __atomic_sub_fetch(&owner->retired_threads_pending, 1, __ATOMIC_ACQ_REL);
        }
        free(node);
    }

    pcb_t zombie = NULL;
    cow_foreach(process_list, zombie) {
        if (zombie->status != T_ZOMBIE) {
            continue;
        }
        release_process_fds_if_ready(zombie);
    }

    while (true) {
        retired_process_node_t *prev = NULL;
        retired_process_node_t *node = NULL;

        spin_lock(retired_lock);
        for (retired_process_node_t *it = retired_processes; it != NULL; it = it->next) {
            if (__atomic_load_n(&it->process->retired_threads_pending, __ATOMIC_ACQUIRE) == 0
                && it->process->child_threads != NULL && it->process->child_threads->size == 0
                && reclaim_epoch_elapsed(it->epoch)) {
                node = it;
                break;
            }
            prev = it;
        }

        if (node != NULL) {
            if (prev != NULL) {
                prev->next = node->next;
            } else {
                retired_processes = node->next;
            }
        }
        spin_unlock(retired_lock);

        if (node == NULL) {
            break;
        }

        release_process_fds_if_ready(node->process);
        destroy_process(node->process);
        free(node);
    }
}

static void remove_thread_from_process(tcb_t task) {
    if (task == NULL || task->process == NULL || task->process->child_threads == NULL) {
        return;
    }
    if (task->ct_index >= task->process->child_threads->size) {
        return;
    }

    if (cow_list_get(task->process->child_threads, task->ct_index) != task) {
        return;
    }

    cow_list_remove(task->process->child_threads, task->ct_index);
    refresh_child_thread_links(task->process);
}

static pcb_t get_reparent_target(const pcb_t pcb) {
    if (pcb == NULL || pcb->parent == NULL || pcb->parent->parent == NULL) {
        return kernel_process;
    }
    return pcb->parent->parent;
}

static void reparent_process_children(const pcb_t pcb) {
    if (pcb == NULL || pcb->child_process == NULL || pcb->child_process->size == 0) {
        return;
    }

    pcb_t new_parent = get_reparent_target(pcb);
    while (pcb->child_process->size > 0) {
        pcb_t child = cow_list_get(pcb->child_process, 0);
        cow_list_remove(pcb->child_process, 0);
        if (child == NULL) {
            continue;
        }
        child->parent    = new_parent;
        child->ppl_index = cow_list_add(new_parent->child_process, child);
    }
}

static void kill_thread0(pcb_t parent, const tcb_t task) {
    (void)parent;
    task->status = T_OUT;
    enqueue_retired_thread(task);
}

static bool claim_thread_exit(const tcb_t task) {
    spin_lock(task_exit_lock);
    if (task->status == T_DEATH || task->status == T_OUT) {
        spin_unlock(task_exit_lock);
        return false;
    }
    task->status = T_DEATH;
    spin_unlock(task_exit_lock);
    return true;
}

static bool claim_process_zombie_exit(const pcb_t pcb) {
    spin_lock(task_exit_lock);
    if (pcb->status == T_DEATH || pcb->status == T_OUT || pcb->status == T_ZOMBIE) {
        spin_unlock(task_exit_lock);
        return false;
    }
    pcb->status = T_DEATH;
    spin_unlock(task_exit_lock);
    return true;
}

static bool claim_process_reap(const pcb_t pcb) {
    spin_lock(task_exit_lock);
    if (pcb->status != T_ZOMBIE) {
        spin_unlock(task_exit_lock);
        return false;
    }
    pcb->status = T_DEATH;
    spin_unlock(task_exit_lock);
    return true;
}

static void kill_proc0(const pcb_t pcb) {
    pcb->status = T_OUT;
    reparent_process_children(pcb);
    cow_list_remove(pcb->parent->child_process, pcb->ppl_index);
    refresh_child_process_links(pcb->parent);
    while (pcb->child_threads->size > 0) {
        const tcb_t thread = cow_list_get(pcb->child_threads, 0);
        cow_list_remove(pcb->child_threads, 0);
        if (thread == NULL) {
            continue;
        }
        if (thread->status != T_OUT) {
            kill_thread0(pcb, thread);
        }
    }

    cow_list_clear(process_list, pcb->pl_index);

    procfs_on_exit_task(pcb);
    pcb->procfs_node = NULL;

    lazy_free(pcb);

    if (pcb->ipc_queue) {
        ipc_queue_release(pcb->ipc_queue);
        pcb->ipc_queue = NULL;
    }
    if (pcb->virt_queue) {
        free_llist_queue(pcb->virt_queue, NULL, NULL);
        pcb->virt_queue = NULL;
    }
    free(pcb->cmdline);
    pcb->cmdline = NULL;
    if (pcb->cwd) {
        vfs_close(pcb->cwd);
        pcb->cwd = NULL;
    }
    if (pcb->proc_root) {
        vfs_close(pcb->proc_root);
        pcb->proc_root = NULL;
    }

    if (pcb->envp) {
        free_envp(pcb->envp);
        pcb->envp = NULL;
    }
    free(pcb->ctty_path);
    pcb->ctty_path = NULL;
    enqueue_retired_process(pcb);
}

void kill_thread(const tcb_t task) {
    if (task == NULL) {
        return;
    }
    if (!claim_thread_exit(task)) {
        return;
    }
    if (task->tid_directory != NULL) {
        page_directory_t *directory = get_current_directory();
        switch_memory_directory(task->tid_directory);
        uint64_t futex_key = arch_virt_to_phys(task->tid_address);
        if (task->tid_address != 0 && futex_key != 0) {
            int *tid_addr = (int *)task->tid_address;
            *tid_addr     = 0;
        }
        switch_memory_directory(directory);
        task->tid_address   = 0;
        task->tid_directory = NULL;
    }
    futex_free(task);
    if (task->sched_handle != NULL) {
        scheduler_remove_task(task, get_cpu_local(task->cpu_id));
    }
    remove_thread_from_process(task);
    task->status = T_OUT;
    enqueue_retired_thread(task);
}

void kill_proc(const pcb_t pcb, const int exit_code, const bool is_zombie) {
    if (pcb == NULL) {
        return;
    }
    if (pcb->pid == kernel_process->pid) {
        kerror("Cannot kill System process.");
        return;
    }
    if (pcb->tty && pcb->tty->fgproc == pcb->pid) {
        pcb->tty->fgproc = 0;
    }

    if (is_zombie) {
        if (!claim_process_zombie_exit(pcb)) {
            return;
        }
        scheduler_disable();
        for (size_t i = pcb->child_threads->size; i > 0; i--) {
            tcb_t tcb = cow_list_get(pcb->child_threads, i - 1);
            if (tcb == NULL) {
                cow_list_remove(pcb->child_threads, i - 1);
                refresh_child_thread_links(pcb);
                continue;
            }
            if (tcb->status != T_OUT) {
                kill_thread(tcb);
            }
        }
        reparent_process_children(pcb);
        pcb->status             = T_ZOMBIE;
        const ipc_message_t msg = malloc(sizeof(struct ipc_message));
        asserts(msg, "kill_proc: ipc_message is null.");
        msg->pid     = pcb->pid;
        msg->type    = IPC_MSG_TYPE_EPID;
        msg->data[0] = exit_code & 0xFF;
        msg->data[1] = exit_code >> 8 & 0xFF;
        msg->data[2] = exit_code >> 16 & 0xFF;
        msg->data[3] = exit_code >> 24 & 0xFF;
        ipc_send(pcb->parent->ipc_queue, msg);
        send_signal_to_process(pcb->parent, SIGCHLD);

        pcb->parent->cutime += pcb->utime;
        pcb->parent->cstime += pcb->cstime;

        spin_lock(task_exit_lock);
        pcb->status = T_ZOMBIE;
        spin_unlock(task_exit_lock);
        scheduler_enable();
    } else {
        if (!claim_process_reap(pcb)) {
            return;
        }
        kill_proc0(pcb);
    }
}

int waitpid(const pid_t pid, pid_t *pid_ret, const bool nohang) {
    const tcb_t current = get_current_task();
    current->status     = T_WAIT;
    const bool is_sti   = arch_check_interrupt();
    arch_open_interrupt();

    const pcb_t process = current->process;

    ipc_message_t mesg = NULL;
    int exit_code      = 0;
    if (nohang) {
        const size_t tries = process->ipc_queue->size;
        for (size_t i = 0; i < tries; i++) {
            mesg = ipc_recv(process->ipc_queue, IPC_MSG_TYPE_EPID);
            if (mesg == NULL) {
                break;
            }
            exit_code =
                mesg->data[3] << 24 | mesg->data[2] << 16 | mesg->data[1] << 8 | mesg->data[0];
            if (pid == -1 || pid == mesg->pid) {
                break;
            }
            ipc_send(process->ipc_queue, mesg);
            mesg = NULL;
        }
        if (mesg == NULL) {
            if (!is_sti) {
                arch_close_interrupt();
            }
            current->status = T_RUNNING;
            *pid_ret        = 0;
            return 0;
        }
    } else {
        while (1) {
            scheduler_change_weight(current, NICE_TO_PRIO(10));
            mesg = ipc_recv_wait(process->ipc_queue, IPC_MSG_TYPE_EPID);
            scheduler_change_weight(current, NICE_TO_PRIO(0));
            exit_code = (mesg->data[3] << 24) | (mesg->data[2] << 16) | (mesg->data[1] << 8)
                        | mesg->data[0];
            if (pid == -1 || pid == mesg->pid) {
                break;
            }
            ipc_send(process->ipc_queue, mesg);
        }
    }

    const pcb_t wait_p = found_pcb(mesg->pid);
    if (wait_p && wait_p->status == T_ZOMBIE) {
        kill_proc(wait_p, exit_code, false);
    }
    *pid_ret = mesg->pid;
    free(mesg);

    if (!is_sti) {
        arch_close_interrupt();
    }
    current->status = T_RUNNING;
    return exit_code;
}

pid_t create_process(const char *name, pcb_t parent, uint64_t flags) {
    const pcb_t new_pgb = calloc(1, sizeof(struct process_control_block));
    if (new_pgb == NULL) {
        return -ENOMEM;
    }
    new_pgb->name          = strdup(name);
    new_pgb->pl_index      = cow_list_add(process_list, new_pgb);
    new_pgb->pid           = alloc_pid();
    new_pgb->parent        = parent == NULL ? kernel_process : parent;
    new_pgb->umask         = new_pgb->parent ? new_pgb->parent->umask : 0022;
    new_pgb->child_threads = cow_list_create();
    new_pgb->tty           = new_pgb->parent->tty;
    new_pgb->ctty_path     = new_pgb->parent->ctty_path ? strdup(new_pgb->parent->ctty_path) : NULL;
    new_pgb->fdts          = fds_init();
    new_pgb->ipc_queue     = ipc_queue_init();
    new_pgb->virt_queue    = create_llist_queue();
    new_pgb->cwd           = get_rootdir();
    new_pgb->child_process = cow_list_create();
    new_pgb->ppl_index     = cow_list_add(new_pgb->parent->child_process, new_pgb);
    new_pgb->vfork         = false;
    new_pgb->proc_root     = get_rootdir();
    if (flags & CLONE_VM) {
        new_pgb->mm = new_pgb->parent->mm;
        mm_retain(new_pgb->mm);
    } else {
        new_pgb->mm = mm_clone(new_pgb->parent->mm);
    }
    if (new_pgb->mm == NULL) {
        cow_list_remove(new_pgb->parent->child_process, new_pgb->ppl_index);
        refresh_child_process_links(new_pgb->parent);
        cow_list_clear(process_list, new_pgb->pl_index);
        cow_list_destroy(new_pgb->child_threads);
        cow_list_destroy(new_pgb->child_process);
        ipc_queue_release(new_pgb->ipc_queue);
        free_llist_queue(new_pgb->virt_queue, NULL, NULL);
        free_fdt(new_pgb->fdts);
        if (new_pgb->cwd != NULL) {
            vfs_close(new_pgb->cwd);
        }
        if (new_pgb->proc_root != NULL) {
            vfs_close(new_pgb->proc_root);
        }
        free(new_pgb->ctty_path);
        free(new_pgb->name);
        free((void *)new_pgb);
        return -ENOMEM;
    }
    return new_pgb->pid;
}

pid_t create_kernel_thread(
    const char *name, int (*func)(void *arg), void *arg, pcb_t process, uint64_t prio
) {
    const tcb_t thread = calloc(1, STACK_SIZE);
    asserts(thread, "create kernel thread null.");
    thread->name     = strdup(name);
    thread->tid      = alloc_tid();
    thread->process  = process == NULL ? kernel_process : process;
    thread->ct_index = cow_list_add(thread->process->child_threads, thread);
    thread->prio     = prio;
    thread->_start   = (uint64_t)func;
    thread->status   = T_CREATE;
    thread->signal_stack =
        (uint64_t)phys_to_virt((alloc_frames(MAX_STACK_SIZE / PAGE_SIZE) + MAX_STACK_SIZE));
    ;
    thread->syscall_stack =
        (uint64_t)phys_to_virt((alloc_frames(MAX_STACK_SIZE / PAGE_SIZE) + MAX_STACK_SIZE));
    ;
    arch_context_init_thread(thread, arg);
    scheduler_add_task(thread, thread->prio);
    return thread->tid;
}

void task_refresh_tick_work_state(pcb_t task) {
    bool active = false;

    if (!task) {
        return;
    }

    if (task->itimer_real.at) {
        active = true;
        goto out;
    }

    for (int i = 0; i < MAX_TIMERS_NUM; i++) {
        const kernel_timer_t *kt = task->timers[i];

        if (kt && kt->expires) {
            active = true;
            break;
        }
    }

out:
    task->tick_work_active = active;
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
    kernel_process->mm            = mm_create(get_kernel_pagedir());
    kernel_process->tty           = get_kernel_session();
    kernel_process->ctty_path     = strdup("/dev/tty0");
    kernel_process->status        = T_RUNNING;
    kernel_process->exec          = NULL;
    kernel_process->virt_queue    = create_llist_queue();
    kernel_process->fdts          = fds_init();
    kernel_process->child_process = cow_list_create();
    kernel_process->ipc_queue     = ipc_queue_init();
    kernel_process->vfork         = false;
    kernel_process->umask         = 0022;
    asserts(kernel_process->mm != NULL, "setup_task: kernel mm is null.");

    bsp_idle_thread           = malloc(STACK_SIZE);
    bsp_idle_thread->name     = strdup("bsp_idle");
    bsp_idle_thread->process  = kernel_process;
    bsp_idle_thread->tid      = alloc_tid();
    bsp_idle_thread->ct_index = cow_list_add(kernel_process->child_threads, bsp_idle_thread);
    bsp_idle_thread->status   = T_RUNNING;
    bsp_idle_thread->signal_stack =
        (uint64_t)phys_to_virt(alloc_frames(MAX_STACK_SIZE / PAGE_SIZE) + MAX_STACK_SIZE);

    bsp_idle_thread->syscall_stack =
        (uint64_t)phys_to_virt(alloc_frames(MAX_STACK_SIZE / PAGE_SIZE) + MAX_STACK_SIZE);

    arch_context_init(bsp_idle_thread, &bsp_idle_thread->context);
    kinfo("kernel process(%s) PID: %d ", kernel_process->name, kernel_process->pid);
}
