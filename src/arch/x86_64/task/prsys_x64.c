#include "errno.h"
#include "fs/procfs.h"
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

    tcb_t current = get_current_task();
    pcb_t current_pcb = current->process;

    pcb_t new_pcb = malloc(sizeof(struct process_control_block));
    memset(new_pcb, 0, sizeof(struct process_control_block));
    new_pcb->pid    = alloc_pid();
    new_pcb->name   = strdup(current_pcb->name);
    new_pcb->status = T_START;
    new_pcb->tty    = current_pcb->tty;
    new_pcb->pgid   = current_pcb->pgid;
    new_pcb->sid    = current_pcb->sid;

    new_pcb->directory =
        is_vfork ? current_pcb->directory : clone_page_directory(current_pcb->directory, false);
    if (!vma_manager_clone(&current_pcb->vma_manager, &new_pcb->vma_manager)) {
        logkf("task: cannot clone process vma information.\n");
        free(new_pcb->name);
        free(new_pcb);
        return -ENOMEM;
    }
    tcb_t parent_task = current;
    tcb_t new_task    = malloc(STACK_SIZE);
    if (new_task == NULL) {
        vma_manager_exit_cleanup(&new_pcb->vma_manager);
        free(new_pcb->name);
        free(new_pcb);
        return SYSCALL_FAULT_(ENOMEM);
    }
    memset(new_task, 0, sizeof(struct thread_control_block));

    new_pcb->exec = current_pcb->exec;
    new_pcb->exec->refcount++;

    new_pcb->cmdline       = strdup(current_pcb->cmdline);
    new_pcb->ipc_queue     = ipc_queue_init();
    new_pcb->cwd           = current_pcb->cwd;
    new_pcb->cwd->refcount++;
    new_pcb->parent     = current_pcb;
    new_pcb->virt_queue = copy_list_queue(current_pcb->virt_queue, virt_copy, virt_copy_index);
    // new_pcb->mmap_start    = current_pcb->mmap_start;
    new_pcb->fdts          = copy_fdt(current_pcb->fdts);
    new_pcb->pl_index      = cow_list_add(process_list, new_pcb);
    new_pcb->vfork         = is_vfork;
    new_pcb->child_process = cow_list_create();
    new_pcb->child_threads = cow_list_create();
    new_pcb->ppl_index     = cow_list_add(current_pcb->child_process, new_pcb);
    new_pcb->proc_root     = current_pcb->proc_root;
    new_pcb->proc_root->refcount++;
    new_pcb->ctty_path     = current_pcb->ctty_path ? strdup(current_pcb->ctty_path) : NULL;

    new_task->cpu_id                 = current_cpu->id;
    new_task->status                 = T_START;
    new_task->context.user_stack     = parent_task->context.user_stack;
    new_task->context.user_stack_top = parent_task->context.user_stack_top;
    new_task->context.kernel_stack   = ((uint64_t)new_task) + STACK_SIZE;
    new_task->_start                 = parent_task->_start;
    new_task->name                   = strdup(parent_task->name);

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

    new_task->context.context = aligned_alloc(16, sizeof(struct fpu_context));
    memcpy(new_task->context.context, parent_task->context.context, sizeof(fpu_context_t));

    new_task->affinity_mask   = parent_task->affinity_mask;
    new_task->context.fs      = parent_task->context.fs;
    new_task->context.fs_base = parent_task->context.fs_base;

    // Inherit signal handlers and blocked mask from parent (POSIX fork semantics)
    memcpy(new_task->actions, parent_task->actions, sizeof(parent_task->actions));
    new_task->blocked = parent_task->blocked;

    new_task->process  = new_pcb;
    new_task->ct_index = cow_list_add(new_pcb->child_threads, new_task);
    new_task->tid      = alloc_tid();

    new_task->tid_address   = parent_task->tid_address;
    new_task->tid_directory = parent_task->tid_directory;

    void *signal_stack  = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    void *syscall_stack = aligned_alloc(PAGE_SIZE, MAX_STACK_SIZE) + MAX_STACK_SIZE;
    memset((void *)(signal_stack - STACK_SIZE), 0, STACK_SIZE);
    memset((void *)(syscall_stack - MAX_STACK_SIZE), 0, MAX_STACK_SIZE);
    new_task->signal_stack  = (uint64_t)signal_stack;
    new_task->syscall_stack = (uint64_t)syscall_stack;

    scheduler_add_task(new_task, NICE_TO_PRIO(0));
    scheduler_enable();
    arch_open_interrupt();
    procfs_on_new_task(new_pcb);

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
    new_task->name                   =
        parent_task->name ? strdup(parent_task->name) : strdup("thread");
    if (new_task->name == NULL) {
        free(new_task);
        return SYSCALL_FAULT_(ENOMEM);
    }

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

    memcpy(&new_task->context.context, &parent_task->context.context, sizeof(fpu_context_t));

    new_task->affinity_mask   = parent_task->affinity_mask;
    new_task->context.fs      = parent_task->context.fs;
    new_task->context.fs_base = parent_task->context.fs_base;

    // Inherit signal handlers and blocked mask from parent
    memcpy(new_task->actions, parent_task->actions, sizeof(parent_task->actions));
    new_task->blocked = parent_task->blocked;

    void *signal_stack  = aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    void *syscall_stack = aligned_alloc(PAGE_SIZE, MAX_STACK_SIZE) + MAX_STACK_SIZE;
    memset((void *)(signal_stack - STACK_SIZE), 0, STACK_SIZE);
    memset((void *)(syscall_stack - MAX_STACK_SIZE), 0, MAX_STACK_SIZE);
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
    arch_close_interrupt();
    scheduler_disable();
    scheduler_add_task(new_task, parent_task->prio);
    scheduler_enable();
    arch_open_interrupt();

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
    if (unlikely(argv == NULL)) return SYSCALL_FAULT_(EINVAL);

    // If argv is empty ({NULL}), auto-fill argv[0] with the program path (Linux 5.18+ behavior)
    char *auto_argv_buf[2] = {NULL, NULL};
    if (argv[0] == NULL) {
        auto_argv_buf[0] = path;
        argv = auto_argv_buf;
    }

    char      *norm_path     = vfs_cwd_path_build(path);
    char     **shebang_argv  = NULL; // heap-allocated argv (all entries are strdup'd)
    size_t     shebang_argc  = 0;
    int        shebang_depth = 0;

shebang_retry:;
    vfs_node_t node = vfs_open(norm_path);
    if (node == NULL) {
        free(norm_path);
        for (size_t i = 0; i < shebang_argc; i++) free(shebang_argv[i]);
        free(shebang_argv);
        return SYSCALL_FAULT_(ENOENT);
    }

    // Shebang (#!) check
    if (shebang_depth < 4) {
        char shebang_buf[256];
        size_t n = vfs_read(node, shebang_buf, 0, sizeof(shebang_buf) - 1);
        if (n >= 4 && shebang_buf[0] == '#' && shebang_buf[1] == '!') {
            shebang_buf[n] = '\0';
            char *nl = strchr(shebang_buf, '\n');
            if (nl) *nl = '\0';

            // Skip whitespace after #!
            char *interp = shebang_buf + 2;
            while (*interp == ' ' || *interp == '\t') interp++;

            // Trim trailing whitespace / CR
            size_t ilen = strlen(interp);
            while (ilen > 0 && (interp[ilen - 1] == ' ' || interp[ilen - 1] == '\t' ||
                                interp[ilen - 1] == '\r'))
                interp[--ilen] = '\0';

            // Split interpreter and optional argument
            char *opt_arg = NULL;
            char *sp = interp;
            while (*sp && *sp != ' ' && *sp != '\t') sp++;
            if (*sp) {
                *sp = '\0';
                opt_arg = sp + 1;
                while (*opt_arg == ' ' || *opt_arg == '\t') opt_arg++;
                if (*opt_arg == '\0') opt_arg = NULL;
                if (opt_arg) {
                    size_t alen = strlen(opt_arg);
                    while (alen > 0 && (opt_arg[alen - 1] == ' ' || opt_arg[alen - 1] == '\t' ||
                                        opt_arg[alen - 1] == '\r'))
                        opt_arg[--alen] = '\0';
                }
            }

            if (*interp == '\0') {
                vfs_close(node);
                free(norm_path);
                for (size_t i = 0; i < shebang_argc; i++) free(shebang_argv[i]);
                free(shebang_argv);
                return SYSCALL_FAULT_(ENOENT);
            }

            // Count original argv
            size_t orig_argc = 0;
            while (argv[orig_argc]) orig_argc++;

            // Build new argv: [interp, opt_arg?, script_path, original_argv[1:], NULL]
            // All entries are strdup'd so we can safely free them later
            size_t new_argc = 1 + (opt_arg ? 1 : 0) + 1 + (orig_argc > 1 ? orig_argc - 1 : 0);
            char **new_argv = malloc((new_argc + 1) * sizeof(char *));
            size_t idx = 0;
            new_argv[idx++] = strdup(interp);
            if (opt_arg) new_argv[idx++] = strdup(opt_arg);
            new_argv[idx++] = strdup(norm_path); // script full path
            for (size_t i = 1; i < orig_argc; i++)
                new_argv[idx++] = strdup(argv[i]);
            new_argv[idx] = NULL;

            vfs_close(node);

            // Free previous shebang_argv
            for (size_t i = 0; i < shebang_argc; i++) free(shebang_argv[i]);
            free(shebang_argv);
            free(norm_path);

            norm_path     = strdup(new_argv[0]); // interpreter path for next open
            argv          = new_argv;
            shebang_argv  = new_argv;
            shebang_argc  = new_argc;
            shebang_depth++;
            goto shebang_retry;
        }
    }

    tcb_t current = get_current_task();
    pcb_t process = current->process;

    arch_close_interrupt();
    scheduler_disable();

    char *old_cmdline = process->cmdline;
    process->cmdline  = build_proc_cmdline(argv, &process->cl_length);
    if (process->name != NULL) free(process->name);
    process->name = malloc(50);
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
        if (handle != NULL && handle->flags & O_CLOEXEC) {
            vfs_close(handle->node);
            remove_fd(process->fdts, handle->fd);
        }
    }

    // POSIX execve: reset signal handlers with user functions to SIG_DFL
    // SIG_IGN and SIG_DFL are preserved; blocked mask is preserved
    {
        for (int i = MINSIG; i <= MAXSIG; i++) {
            if (current->actions[i].sa_handler != SIG_IGN &&
                current->actions[i].sa_handler != SIG_DFL) {
                current->actions[i].sa_handler = SIG_DFL;
                current->actions[i].sa_flags   = 0;
                current->actions[i].sa_mask    = 0;
                current->actions[i].sa_restorer = NULL;
            }
        }
        current->signal = 0; // clear pending signals
    }

    // 根据 POSIX 的 execve 规范定义, 内核对象不变, 故懒分配器, IPC等不动
    //    lazy_free(process);
    //    process->virt_queue = create_llist_queue();
    //
    //    ipc_queue_release(process->ipc_queue);
    //    process->ipc_queue = ipc_queue_init();

    free(norm_path);
    for (size_t i = 0; i < shebang_argc; i++) free(shebang_argv[i]);
    free(shebang_argv);
    free_envp(old_envp);

    uint64_t stack = page_alloc_random(get_current_directory(), BIG_USER_STACK,
                                       PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    current->context.user_stack     = stack;
    current->context.user_stack_top = stack + BIG_USER_STACK;
    current->tid_directory          = NULL;
    current->tid_address            = 0;

    scheduler_enable();
    arch_open_interrupt();
    arch_switch_to_user_mode();
}

syscall_(clone, uint64_t flags, uint64_t stack, int *parent_tid, int *child_tid, uint64_t tls) {
    uint64_t id = thread_clone(regs, flags, stack, parent_tid, child_tid, tls);
    return id;
}
