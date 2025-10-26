#include "errno.h"
#include "fsgsbase.h"
#include "mem/lazy_alloc.h"
#include "nr.h"
#include "syscall.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

extern cow_arraylist *process_list;

static uint64_t process_fork(struct syscall_regs *reg, bool is_vfork, uint64_t user_stack) {
    cpu_local_t *current_cpu = arch_current_cpu();

    pcb_t current_pcb = get_current_task()->process;

    pcb_t new_pcb = malloc(sizeof(struct process_control_block));
    memset(new_pcb, 0, sizeof(struct process_control_block));
    new_pcb->pid = alloc_pid();
    strcpy(new_pcb->name, current_pcb->name);
    new_pcb->status = T_START;
    new_pcb->tty    = current_pcb->tty;

    new_pcb->directory =
        is_vfork ? current_pcb->directory : clone_page_directory(current_pcb->directory, false);
    if (!vma_manager_clone(&current_pcb->vma_manager, &new_pcb->vma_manager)) {
        logkf("task: cannot clone process vma information.\n");
        free(new_pcb);
        return -ENOMEM;
    }
    new_pcb->exec = current_pcb->exec;
    new_pcb->exec->refcount++;

    new_pcb->cmdline       = strdup(current_pcb->cmdline);
    new_pcb->child_process = cow_list_create();
    new_pcb->ipc_queue     = ipc_queue_init();
    new_pcb->cwd           = current_pcb->cwd;
    new_pcb->cwd->refcount++;
    new_pcb->parent     = current_pcb;
    new_pcb->virt_queue = copy_list_queue(current_pcb->virt_queue, virt_copy);
    // new_pcb->mmap_start    = current_pcb->mmap_start;
    new_pcb->fdts          = copy_fdt(current_pcb->fdts);
    new_pcb->pl_index      = cow_list_add(process_list, new_pcb);
    new_pcb->vfork         = is_vfork;
    new_pcb->child_process = cow_list_create();
    new_pcb->child_threads = cow_list_create();
    new_pcb->ppl_index     = cow_list_add(current_pcb->child_process, new_pcb);

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(STACK_SIZE);
    if (new_task == NULL) return SYSCALL_FAULT_(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->cpu_id                 = current_cpu->id;
    new_task->status                 = T_START;
    new_task->context.user_stack     = parent_task->context.user_stack;
    new_task->context.user_stack_top = parent_task->context.user_stack_top;
    new_task->context.kernel_stack   = ((uint64_t)new_task) + STACK_SIZE;
    new_task->_start                 = parent_task->_start;
    strcpy(new_task->name, parent_task->name);

    new_task->context.regs.rip    = reg->rcx; // syscall 指令中 rcx 寄存器为 rip
    new_task->context.regs.rflags = reg->r11; // syscall 指令中 r11 寄存器为 rflags
    new_task->context.regs.cs     = reg->cs;
    new_task->context.regs.ss     = reg->ss;
    new_task->context.regs.es     = reg->es;
    new_task->context.regs.ds     = reg->ds;
    new_task->context.regs.rax    = 0; // 子线程返回 0
    new_task->context.regs.rdi    = reg->rdi;
    new_task->context.regs.rsi    = reg->rsi;
    new_task->context.regs.rdx    = reg->rdx;
    new_task->context.regs.r9     = reg->r9;
    new_task->context.regs.r8     = reg->r8;
    new_task->context.regs.r10    = reg->r10;
    new_task->context.regs.r11    = reg->r11;
    new_task->context.regs.r12    = reg->r12;
    new_task->context.regs.r13    = reg->r13;
    new_task->context.regs.r14    = reg->r14;
    new_task->context.regs.r15    = reg->r15;
    new_task->context.regs.rbx    = reg->rbx;
    new_task->context.regs.rbp    = reg->rbp;
    new_task->context.regs.rsp    = user_stack == 0 ? reg->rsp : user_stack;
    new_task->context.regs.rcx    = reg->rcx;

    memcpy(new_task->context.context.fxsave_area, parent_task->context.context.fxsave_area, 512);

    new_task->affinity_mask   = parent_task->affinity_mask;
    new_task->context.fs      = parent_task->context.fs;
    new_task->context.fs_base = parent_task->context.fs_base;

    new_task->process  = new_pcb;
    new_task->ct_index = cow_list_add(new_pcb->child_threads, new_task);
    new_task->tid      = alloc_tid();

    new_task->tid_address   = parent_task->tid_address;
    new_task->tid_directory = parent_task->tid_directory;

    void *signal_stack  = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    void *syscall_stack = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    memset((void *)(signal_stack - STACK_SIZE), 0, STACK_SIZE);
    memset((void *)(syscall_stack - STACK_SIZE), 0, STACK_SIZE);
    new_task->signal_stack  = (uint64_t)signal_stack;
    new_task->syscall_stack = (uint64_t)syscall_stack;

    add_task_prio(new_task, NICE_TO_PRIO(0));
    enable_scheduler();
    arch_open_interrupt();
    // procfs_on_new_task(new_pcb);

    if (!is_vfork) return new_pcb->pid;
    while (true) {
        ipc_message_t msg =
            ipc_recv_wait2(current_pcb->ipc_queue, IPC_MSG_TYPE_EXEC, IPC_MSG_TYPE_EPID);
        if (msg->pid == new_pcb->pid) {
            free(msg);
            return new_pcb->pid;
        }
        ipc_send(current_pcb->parent->ipc_queue, msg);
    }

    int npid = new_pcb->pid;
    do {
        ipc_message_t msg = ipc_recv_wait(current_pcb->ipc_queue, IPC_MSG_TYPE_EXEC);
        if (npid == msg->pid) {
            free(msg);
            return npid;
        }
        ipc_send(current_pcb->parent->ipc_queue, msg);
    } while (true);
}

uint64_t thread_clone(struct syscall_regs *reg, uint64_t flags, uint64_t stack, int *parent_tid,
                      int *child_tid, uint64_t tls) {

    if (flags & CLONE_VFORK) { return process_fork(reg, true, stack); }

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(STACK_SIZE);
    if (new_task == NULL) return SYSCALL_FAULT_(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->cpu_id                 = arch_current_cpu()->id;
    new_task->status                 = T_START;
    new_task->context.regs.rsp       = stack;
    new_task->context.user_stack     = stack;
    new_task->context.user_stack_top = new_task->context.regs.rsp;
    new_task->context.regs.rflags    = reg->rflags;
    new_task->context.kernel_stack   = (uint64_t)new_task + STACK_SIZE;
    new_task->_start                 = parent_task->_start;
    strcpy(new_task->name, parent_task->name);

    new_task->context.regs.rip    = reg->rcx; // syscall 指令中 rcx 寄存器为 rip
    new_task->context.regs.rflags = reg->r11; // syscall 指令中 r11 寄存器为 rflags
    new_task->context.regs.cs     = reg->cs;
    new_task->context.regs.ss     = reg->ss;
    new_task->context.regs.es     = reg->es;
    new_task->context.regs.ds     = reg->ds;
    new_task->context.regs.rax    = 0; // 子线程返回 0
    new_task->context.regs.rdi    = reg->rdi;
    new_task->context.regs.rsi    = reg->rsi;
    new_task->context.regs.rdx    = reg->rdx;
    new_task->context.regs.r9     = reg->r9;
    new_task->context.regs.r8     = reg->r8;
    new_task->context.regs.r10    = reg->r10;
    new_task->context.regs.r11    = reg->r11;
    new_task->context.regs.r12    = reg->r12;
    new_task->context.regs.r13    = reg->r13;
    new_task->context.regs.r14    = reg->r14;
    new_task->context.regs.r15    = reg->r15;
    new_task->context.regs.rbx    = reg->rbx;
    new_task->context.regs.rbp    = reg->rbp;
    new_task->context.regs.rcx    = reg->rcx;

    memcpy(new_task->context.context.fxsave_area, parent_task->context.context.fxsave_area, 512);

    new_task->affinity_mask   = parent_task->affinity_mask;
    new_task->context.fs      = parent_task->context.fs;
    new_task->context.fs_base = parent_task->context.fs_base;

    void *signal_stack  = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    void *syscall_stack = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    memset((void *)(signal_stack - STACK_SIZE), 0, STACK_SIZE);
    memset((void *)(syscall_stack - STACK_SIZE), 0, STACK_SIZE);
    new_task->signal_stack  = (uint64_t)signal_stack;
    new_task->syscall_stack = (uint64_t)syscall_stack;

    new_task->process  = parent_task->process;
    new_task->ct_index = cow_list_add(parent_task->process->child_threads, new_task);
    new_task->tid      = alloc_tid();

    if (flags & CLONE_SETTLS) { new_task->context.fs_base = tls; }

    if (flags & CLONE_PARENT_SETTID) { *parent_tid = new_task->tid; }

    if (flags & CLONE_CHILD_SETTID) { *child_tid = new_task->tid; }

    if (flags & CLONE_CHILD_CLEARTID) {
        new_task->tid_address   = (uint64_t)child_tid;
        new_task->tid_directory = get_current_directory();
    }
    add_task_prio(new_task, parent_task->prio);

    return new_task->tid;
}

syscall_(arch_prctl, uint64_t code, uint64_t addr) {
    tcb_t thread = get_current_task();
    switch (code) {
    case ARCH_SET_FS:
        thread->context.fs_base = addr;
        write_fsbase(thread->context.fs_base);
        break;
    case ARCH_GET_FS: return thread->context.fs_base;
    case ARCH_SET_GS:
        thread->context.gs_base = addr;
        write_gsbase(thread->context.gs_base);
        break;
    case ARCH_GET_GS: return thread->context.gs_base;
    default: return -EINVAL;
    }
    return EOK;
}

syscall_(fork) {
    return process_fork(regs, false, 0);
}

syscall_(vfork) {
    return process_fork(regs, true, 0);
}

syscall_(execve, char *path, char **argv, char **envp) {
    if (unlikely(path == NULL)) return SYSCALL_FAULT_(EINVAL);
    char      *norm_path = vfs_cwd_path_build(path);
    vfs_node_t node      = vfs_open(norm_path);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    uint64_t buf_len = (node->size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    pcb_t process = get_current_task()->process;

    arch_close_interrupt();
    disable_scheduler();

    //
    //    if (strncmp(pcb_buffer, "#!", 2) == 0) {
    //        int   interpreter_argc;
    //        char *interpreter_argv[50];
    //        char  interpreter_buffer[1024];
    //        *strchr(pcb_buffer, '\n') = '\0';
    //        strcpy(interpreter_buffer, pcb_buffer);
    //        strcat(interpreter_buffer, " ");
    //        strcat(interpreter_buffer, path);
    //        interpreter_argc = cmd_parse(interpreter_buffer, interpreter_argv, ' ');
    //
    //        free(pcb_buffer);
    //        vfs_close(node);
    //        free(norm_path);
    //        return process_execve(interpreter_argv[0], interpreter_argv, envp);
    //        //TODO cmd_parse 无合理释放的区域, 会造成内存泄漏, 等待修复
    //    }

    char cmdline[PAGE_SIZE];
    memset(cmdline, 0, sizeof(cmdline));
    char *cmdline_ptr = cmdline;
    if (argv == NULL) {
        free(norm_path);
        enable_scheduler();
        arch_open_interrupt();
        return SYSCALL_FAULT_(EINVAL);
    }

    for (int i = 0; argv[i]; i++) {
        int len      = sprintf(cmdline_ptr, "%s ", argv[i]);
        cmdline_ptr += len;
    }

    char *old_cmdline = process->cmdline;
    process->cmdline  = strdup(cmdline);
    strncpy(process->name, norm_path, 50);

    char **old_envp = process->envp;
    size_t old_envc = process->envc;
    process->envp   = copy_envp(envp);
    process->envc   = envp_length(envp);

    if (!process->vfork) vma_manager_exit_cleanup(&process->vma_manager);
    page_directory_t *old_page_dir = process->directory;
    switch_context_directory(clone_page_directory(get_kernel_pagedir(), false));

    if (old_cmdline) free(old_cmdline);

    if (process->vfork) {
        ipc_message_t message = calloc(1, sizeof(struct ipc_message));
        message->type         = IPC_MSG_TYPE_EXEC;
        message->pid          = process->pid;
        ipc_send(process->parent->ipc_queue, message);
    }

    if (!process->vfork) free_page_directory(old_page_dir);
    process->directory = get_current_directory();
    process->vfork     = false;

    process->exec = node;

    for (size_t i = 0; i < process->fdts->fds_length; i++) {
        fd_t *handle = process->fdts->fds[i];
        if (handle != NULL) {
            vfs_close(handle->node);
            free(handle);
        }
    }

    free_fdt(process->fdts);
    process->fdts = fds_init();

    lazy_free(process);
    process->virt_queue = create_llist_queue();

    ipc_queue_release(process->ipc_queue);
    process->ipc_queue = ipc_queue_init();

    free(norm_path);

    uint64_t stack = page_alloc_random(get_current_directory(), BIG_USER_STACK,
                                       PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    get_current_task()->context.user_stack     = stack;
    get_current_task()->context.user_stack_top = stack + BIG_USER_STACK;
    get_current_task()->tid_directory          = NULL;
    get_current_task()->tid_address            = 0;

    enable_scheduler();
    arch_open_interrupt();
    arch_switch_to_user_mode();
}

syscall_(clone, uint64_t flags, uint64_t stack, int *parent_tid, int *child_tid, uint64_t tls) {
    arch_close_interrupt();
    disable_scheduler();
    uint64_t id = thread_clone(regs, flags, stack, parent_tid, child_tid, tls);
    arch_open_interrupt();
    enable_scheduler();
    return id;
}
