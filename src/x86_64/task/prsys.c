/**
 * @file prsys.c
 * 多任务相关系统调用实现
 * 非常不建议内核态调用这里的函数, 此函数仅供用户态使用
 * 内核态操作进程请使用 pcb.c 中的函数
 */
#include "elf_util.h"
#include "fsgsbase.h"
#include "heap.h"
#include "hhdm.h"
#include "ipc.h"
#include "klog.h"
#include "pcb.h"
#include "smp.h"
#include "sprintf.h"
#include "vfs.h"

extern int         now_tid;
extern int         now_pid;
extern lock_queue *pgb_queue;

uint64_t thread_clone(struct syscall_regs *reg, uint64_t flags, uint64_t stack, int *parent_tid,
                      int *child_tid, uint64_t tls) {

    if (flags & CLONE_VFORK) { return process_fork(reg, true, stack); }

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(STACK_SIZE);
    if (new_task == NULL) return SYSCALL_FAULT_(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->task_level      = TASK_APPLICATION_LEVEL;
    new_task->cpu_clock       = 0;
    new_task->cpu_timer       = 0;
    new_task->mem_usage       = get_all_memusage();
    new_task->cpu_id          = current_cpu->id;
    new_task->status          = START;
    new_task->context0.rsp    = stack;
    new_task->user_stack      = stack;
    new_task->user_stack_top  = new_task->context0.rsp;
    new_task->context0.rflags = reg->rflags;
    new_task->kernel_stack    = (uint64_t)new_task + STACK_SIZE;
    new_task->main            = parent_task->main;
    strcpy(new_task->name, parent_task->name);

    new_task->context0.rip    = reg->rcx; // syscall 指令中 rcx 寄存器为 rip
    new_task->context0.rflags = reg->r11; // syscall 指令中 r11 寄存器为 rflags
    new_task->context0.cs     = reg->cs;
    new_task->context0.ss     = reg->ss;
    new_task->context0.es     = reg->es;
    new_task->context0.ds     = reg->ds;
    new_task->context0.rax    = 0; // 子线程返回 0
    new_task->context0.rdi    = reg->rdi;
    new_task->context0.rsi    = reg->rsi;
    new_task->context0.rdx    = reg->rdx;
    new_task->context0.r9     = reg->r9;
    new_task->context0.r8     = reg->r8;
    new_task->context0.r10    = reg->r10;
    new_task->context0.r11    = reg->r11;
    new_task->context0.r12    = reg->r12;
    new_task->context0.r13    = reg->r13;
    new_task->context0.r14    = reg->r14;
    new_task->context0.r15    = reg->r15;
    new_task->context0.rbx    = reg->rbx;
    new_task->context0.rbp    = reg->rbp;
    new_task->context0.rcx    = reg->rcx;

    memcpy(new_task->fpu_context.fxsave_area, parent_task->fpu_context.fxsave_area, 512);
    new_task->fpu_flags = parent_task->fpu_flags;

    new_task->affinity_mask = parent_task->affinity_mask;
    new_task->fs            = parent_task->fs;
    new_task->fs_base       = parent_task->fs_base;

    new_task->parent_group = parent_task->parent_group;
    new_task->group_index  = lock_queue_enqueue(parent_task->parent_group->pcb_queue, new_task);
    spin_unlock(parent_task->parent_group->pcb_queue->lock);
    new_task->tid          = now_tid++;
    new_task->parent_group = parent_task->parent_group;

    if (flags & CLONE_SETTLS) { new_task->fs_base = tls; }

    if (flags & CLONE_PARENT_SETTID) { *parent_tid = new_task->tid; }

    if (flags & CLONE_CHILD_SETTID) { *child_tid = new_task->tid; }

    if (flags & CLONE_CHILD_CLEARTID) {
        new_task->tid_address   = (uint64_t)child_tid;
        new_task->tid_directory = get_current_directory();
    }
    add_task(new_task);

    return new_task->tid;
}

int process_control(int option, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (option) {
    case PR_SET_NAME:
        if (arg2 == 0) return -1;
        char *new_name = (char *)arg2;
        int   length   = strlen(new_name);
        if (length > 16) return -1;
        memcpy(get_current_task()->name, new_name, length);
        break;
    case PR_GET_NAME:
        if (arg2 == 0) return -1;
        char *proc_name = (char *)arg2;
        memcpy(proc_name, get_current_task()->name, 16);
        break;
    case PR_GET_DUMPABLE: return 0; //TODO CP_Kernel 不支持核心转储
    case PR_SET_DUMPABLE: return -1;
    default: return -1;
    }
    return SYSCALL_SUCCESS;
}

static void *virt_copy(void *ptr) {
    if (ptr == NULL) return NULL;
    mm_virtual_page_t *src_page = (mm_virtual_page_t *)ptr;
    mm_virtual_page_t *new_page = (mm_virtual_page_t *)malloc(sizeof(mm_virtual_page_t));
    new_page->start             = src_page->start;
    new_page->flags             = src_page->flags;
    new_page->count             = src_page->count;
    new_page->pte_flags         = src_page->pte_flags;
    new_page->index             = src_page->index;
    return new_page;
}

extern fd_file_handle *fd_dup(fd_file_handle *src);

static void *file_copy(void *ptr) {
    if (ptr == NULL) return NULL;
    fd_file_handle *src = ptr;
    return fd_dup(src);
}

uint64_t process_fork(struct syscall_regs *reg, bool is_vfork, uint64_t user_stack) {
    //    close_interrupt;
    //    disable_scheduler();

    pcb_t current_pcb = get_current_task()->parent_group;

    pcb_t new_pcb = malloc(sizeof(struct process_control_block));
    memset(new_pcb, 0, sizeof(struct process_control_block));
    new_pcb->pid = now_pid++;
    strcpy(new_pcb->name, current_pcb->name);
    new_pcb->task_level = TASK_APPLICATION_LEVEL;
    new_pcb->status     = START;
    new_pcb->tty        = alloc_default_tty();

    new_pcb->page_dir =
        is_vfork ? current_pcb->page_dir : clone_page_directory(current_pcb->page_dir, false);

    new_pcb->elf_file = malloc(current_pcb->elf_size);
    memcpy(new_pcb->elf_file, current_pcb->elf_file, current_pcb->elf_size);
    new_pcb->elf_size  = current_pcb->elf_size;
    new_pcb->cmdline   = strdup(current_pcb->cmdline);
    new_pcb->pcb_queue = queue_init();
    new_pcb->ipc_queue = queue_init();
    new_pcb->user      = current_pcb->user;
    new_pcb->cwd       = malloc(strlen(current_pcb->cwd));
    strcpy(new_pcb->cwd, current_pcb->cwd);
    new_pcb->parent_task = current_pcb;
    new_pcb->virt_queue  = queue_copy(current_pcb->virt_queue, virt_copy);
    new_pcb->mmap_start  = current_pcb->mmap_start;
    new_pcb->file_open   = queue_copy(current_pcb->file_open, file_copy);
    new_pcb->queue_index = queue_enqueue(pgb_queue, new_pcb);
    new_pcb->vfork       = is_vfork;
    new_pcb->child_pcb   = queue_init();
    new_pcb->child_index = queue_enqueue(current_pcb->child_pcb, new_pcb);

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(STACK_SIZE);
    if (new_task == NULL) return SYSCALL_FAULT_(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->task_level     = TASK_APPLICATION_LEVEL;
    new_task->cpu_clock      = 0;
    new_task->cpu_timer      = 0;
    new_task->mem_usage      = get_all_memusage();
    new_task->cpu_id         = current_cpu->id;
    new_task->status         = START;
    new_task->user_stack     = parent_task->user_stack;
    new_task->user_stack_top = parent_task->user_stack_top;
    new_task->kernel_stack   = ((uint64_t)new_task) + STACK_SIZE;
    new_task->main           = parent_task->main;
    strcpy(new_task->name, parent_task->name);

    new_task->context0.rip    = reg->rcx; // syscall 指令中 rcx 寄存器为 rip
    new_task->context0.rflags = reg->r11; // syscall 指令中 r11 寄存器为 rflags
    new_task->context0.cs     = reg->cs;
    new_task->context0.ss     = reg->ss;
    new_task->context0.es     = reg->es;
    new_task->context0.ds     = reg->ds;
    new_task->context0.rax    = 0; // 子线程返回 0
    new_task->context0.rdi    = reg->rdi;
    new_task->context0.rsi    = reg->rsi;
    new_task->context0.rdx    = reg->rdx;
    new_task->context0.r9     = reg->r9;
    new_task->context0.r8     = reg->r8;
    new_task->context0.r10    = reg->r10;
    new_task->context0.r11    = reg->r11;
    new_task->context0.r12    = reg->r12;
    new_task->context0.r13    = reg->r13;
    new_task->context0.r14    = reg->r14;
    new_task->context0.r15    = reg->r15;
    new_task->context0.rbx    = reg->rbx;
    new_task->context0.rbp    = reg->rbp;
    new_task->context0.rsp    = user_stack == 0 ? reg->rsp : user_stack;
    new_task->context0.rcx    = reg->rcx;

    memcpy(new_task->fpu_context.fxsave_area, parent_task->fpu_context.fxsave_area, 512);
    new_task->fpu_flags = parent_task->fpu_flags;

    new_task->affinity_mask = parent_task->affinity_mask;
    new_task->fs            = parent_task->fs;
    new_task->fs_base       = parent_task->fs_base;

    new_task->parent_group = new_pcb;
    new_task->group_index  = queue_enqueue(new_pcb->pcb_queue, new_task);
    new_task->tid          = now_tid++;

    new_task->tid_address   = parent_task->tid_address;
    new_task->tid_directory = parent_task->tid_directory;

    add_task(new_task);
    enable_scheduler();
    open_interrupt;

    if (is_vfork) {
        int npid = new_pcb->pid;
        int status;
        waitpid(npid, &status);
    }

    return new_pcb->pid;
}

uint64_t process_execve(char *path, char **argv, char **envp) {
    char      *norm_path = vfs_cwd_path_build(path);
    vfs_node_t node      = vfs_open(norm_path);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    uint64_t buf_len = (node->size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    pcb_t process = get_current_task()->parent_group;

    close_interrupt;
    disable_scheduler();

    char cmdline[PAGE_SIZE];
    memset(cmdline, 0, sizeof(cmdline));
    char *cmdline_ptr = cmdline;
    if (argv == NULL) {
        free(norm_path);
        enable_scheduler();
        open_interrupt;
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

    page_directory_t *old_page_dir = process->page_dir;
    switch_process_page_directory(clone_page_directory(get_kernel_pagedir(), false));

    uint8_t *pcb_buffer = malloc(node->size);
    not_null_assets(pcb_buffer, "process_execve: pcb_buffer alloc null.");
    vfs_read(node, pcb_buffer, 0, node->size);
    uint64_t e_entry    = 0;
    uint64_t load_start = 0;
    e_entry = (uint64_t)load_executor_elf(pcb_buffer, get_current_directory(), 0, &load_start);

    if (e_entry == 0) {
        vfs_close(node);
        free(pcb_buffer);
        free(norm_path);
        free(process->cmdline);
        process->cmdline          = old_cmdline;
        page_directory_t *new_dir = get_current_directory();
        switch_process_page_directory(old_page_dir);
        free_page_directory(new_dir);
        if (old_envp) free(process->envp);
        process->envp = old_envp;
        process->envc = old_envc;
        enable_scheduler();
        open_interrupt;
        return SYSCALL_FAULT_(EINVAL);
    }
    if (old_cmdline) free(old_cmdline);

    if (process->vfork) {
        ipc_message_t message = calloc(1, sizeof(struct ipc_message));
        message->type         = IPC_MSG_TYPE_EPID;
        message->pid          = process->pid;
        ipc_send(process->parent_task, message);
    }

    if (!process->vfork) free_page_directory(old_page_dir);
    process->page_dir = get_current_directory();
    process->vfork    = false;

    process->load_start = load_start;
    process->elf_file   = pcb_buffer;
    process->elf_size   = node->size;

    uint8_t *buffer = (uint8_t *)EHDR_START_ADDR;
    page_map_range_to_random(get_current_directory(), EHDR_START_ADDR, buf_len,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    memcpy(buffer, pcb_buffer, node->size);
    vfs_close(node);

    spin_lock(process->file_open->lock);
    queue_foreach(process->file_open, node) {
        fd_file_handle *file_node = (fd_file_handle *)node->data;
        vfs_close(file_node->node);
        free(file_node);
    }
    spin_unlock(process->file_open->lock);
    queue_destroy(process->file_open);
    process->file_open = queue_init();

    spin_lock(process->virt_queue->lock);
    queue_foreach(process->virt_queue, node) {
        mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
        free(vpage);
    }
    spin_unlock(process->virt_queue->lock);
    queue_destroy(process->virt_queue);
    process->virt_queue = queue_init();

    spin_lock(process->ipc_queue->lock);
    queue_foreach(process->ipc_queue, node) {
        ipc_message_t msg = (ipc_message_t)node->data;
        free(msg);
    }
    spin_unlock(process->ipc_queue->lock);
    queue_destroy(process->ipc_queue);
    process->ipc_queue = queue_init();

    free(norm_path);

    uint64_t stack                     = page_alloc_random(get_current_directory(), BIG_USER_STACK,
                                                           PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    get_current_task()->user_stack     = stack;
    get_current_task()->user_stack_top = stack + BIG_USER_STACK;
    get_current_task()->main           = e_entry;
    get_current_task()->tid_directory  = NULL;
    get_current_task()->tid_address    = 0;
    get_current_task()->cpu_clock      = 0;

    enable_scheduler();
    open_interrupt;
    switch_to_user_mode(e_entry);
    return SYSCALL_FAULT_(EAGAIN);
}
