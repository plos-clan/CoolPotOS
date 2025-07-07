#include "pcb.h"
#include "description_table.h"
#include "elf.h"
#include "elf_util.h"
#include "heap.h"
#include "hhdm.h"
#include "io.h"
#include "ipc.h"
#include "killer.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "module.h"
#include "scheduler.h"
#include "smp.h"
#include "sprintf.h"
#include "timer.h"
#include "vfs.h"

extern tcb_t  kernel_head_task;
spin_t        pcb_lock;
extern spin_t scheduler_lock;

lock_queue *pgb_queue = NULL;

pcb_t kernel_group;

int now_pid = 0;
int now_tid = 0;

_Noreturn void process_exit() {
    uint64_t rax = 0;
    __asm__("movq %%rax,%0" ::"r"(rax) :);
    printk("Kernel thread exit, Code: %d\n", rax);
    kill_thread(get_current_task());
    loop __asm__ volatile("hlt");
}

static uint64_t push_slice(uint64_t ustack, uint8_t *slice, uint64_t len) {
    uint64_t tmp_stack  = ustack;
    tmp_stack          -= len;
    tmp_stack          -= (tmp_stack % 0x08);
    memcpy((void *)tmp_stack, slice, len);
    return tmp_stack;
}

static uint64_t build_user_stack(tcb_t task, uint64_t sp, uint64_t entry_point, cp_module_t *link,
                                 uint64_t link_start) {
    uint64_t env_i  = 0;
    uint64_t argv_i = 0;

    ucb_t user = task->parent_group->user;
    char *argv[50];
    int   argc = cmd_parse(task->parent_group->cmdline, argv, ' ');

    char **envp = user->envp;

    uint64_t tmp_stack = sp;
    tmp_stack          = push_slice(tmp_stack, (uint8_t *)task->name, strlen(task->name) + 1);

    uint64_t execfn_ptr = tmp_stack;

    uint64_t *envps = (uint64_t *)malloc(1024);
    memset(envps, 0, 1024);
    uint64_t *argvps = (uint64_t *)malloc(1024);
    memset(argvps, 0, 1024);

    if (envp != NULL) {
        for (env_i = 0; env_i < user->envc; env_i++) {
            tmp_stack    = push_slice(tmp_stack, (uint8_t *)envp[env_i], strlen(envp[env_i]) + 1);
            envps[env_i] = tmp_stack;
        }
    }

    if (argv != NULL) {
        for (argv_i = 0; argv_i < argc; argv_i++) {
            tmp_stack = push_slice(tmp_stack, (uint8_t *)argv[argv_i], strlen(argv[argv_i]) + 1);
            argvps[argv_i] = tmp_stack;
        }
    }

    uint64_t total_length = 2 * sizeof(uint64_t) + 7 * 2 * sizeof(uint64_t) +
                            (env_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) +
                            (argv_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
    tmp_stack -= (tmp_stack - total_length) % 0x10;

    // push auxv
    uint8_t *tmp = (uint8_t *)malloc(2 * sizeof(uint64_t));
    memset(tmp, 0, 2 * sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    page_map_range_to_random(get_current_directory(), EHDR_START_ADDR,
                             get_current_task()->parent_group->elf_size,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    memcpy((void *)EHDR_START_ADDR, get_current_task()->parent_group->elf_file,
           get_current_task()->parent_group->elf_size);

    if (link != NULL) {
        page_map_range_to_random(get_current_directory(), INTERPRETER_EHDR_ADDR, link->size,
                                 PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
        memcpy((void *)INTERPRETER_EHDR_ADDR, link->data, link->size);
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)EHDR_START_ADDR;
    // CP_Kernel 将用户程序本体从 0 地址加载故不加phdrs的偏移
    Elf64_Phdr *phdrs =
        (Elf64_Phdr *)(ehdr->e_phoff + get_current_task()->parent_group->load_start);

    ((uint64_t *)tmp)[0] = AT_NULL;
    ((uint64_t *)tmp)[1] = 0;

    if (link != NULL) {
        ((uint64_t *)tmp)[0] = AT_PHDR;
        ((uint64_t *)tmp)[1] = (uint64_t)phdrs;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

        ((uint64_t *)tmp)[0] = AT_PHENT;
        ((uint64_t *)tmp)[1] = sizeof(Elf64_Phdr);
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

        ((uint64_t *)tmp)[0] = AT_PHNUM;
        ((uint64_t *)tmp)[1] = ehdr->e_phnum;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));
    }
    ((uint64_t *)tmp)[0] = AT_ENTRY;
    ((uint64_t *)tmp)[1] = entry_point;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_EXECFN;
    ((uint64_t *)tmp)[1] = execfn_ptr;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    if (link_start != 0) {
        ((uint64_t *)tmp)[0] = AT_BASE;
        ((uint64_t *)tmp)[1] = link_start;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));
    }

    ((uint64_t *)tmp)[0] = AT_PAGESZ;
    ((uint64_t *)tmp)[1] = PAGE_SIZE;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    memset(tmp, 0, 2 * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, (uint8_t *)envps, env_i * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, (uint8_t *)argvps, argv_i * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, (uint8_t *)&argv_i, sizeof(uint64_t));

    free(tmp);
    free(envps);
    free(argvps);

    return tmp_stack;
}

void switch_to_user_mode(uint64_t func) {
    close_interrupt;
    uint64_t rsp                        = (uint64_t)get_current_task()->user_stack_top;
    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    func                                = get_current_task()->main;

    elf_start main_func = (elf_start)func;
    if (is_dynamic(get_current_task()->parent_group->elf_file)) {
        logkf("Dynamic %s\n", get_current_task()->name);

        cp_module_t *module = get_module("libc");
        if (module == NULL) {
            kerror("Cannot find libc.so module.");
            process_exit();
        }

        uint64_t  linker_start = UINT64_MAX;
        elf_start linker_main;

        linker_main = load_interpreter_elf(get_current_task()->parent_group->elf_file,
                                           get_current_directory(), &linker_start);

        linker_main = load_executor_elf(module->data, get_current_directory(),
                                        INTERPRETER_BASE_ADDR, &linker_start);
        if (linker_main == NULL) {
            kerror("Cannot load libc.so module.");
            process_exit();
        }
        linker_main = linker_main + linker_start;

        logkf("Linker main: %p | Program main: %p\n", linker_main, func);
        rsp       = build_user_stack(get_current_task(), rsp, func, module, linker_start);
        main_func = linker_main;
    } else {
        rsp = build_user_stack(get_current_task(), rsp, func, NULL, 0);
    }

    vfs_node_t      stdio  = vfs_open("/dev/stdio");
    fd_file_handle *stdout = (fd_file_handle *)malloc(sizeof(fd_file_handle));
    stdout->node           = stdio;
    stdout->offset         = 0;
    fd_file_handle *stdin  = (fd_file_handle *)malloc(sizeof(fd_file_handle));
    stdin->node            = stdio;
    stdin->offset          = 0;
    fd_file_handle *stderr = (fd_file_handle *)malloc(sizeof(fd_file_handle));
    stderr->node           = stdio;
    stderr->offset         = 0;
    stdin->fd  = queue_enqueue(get_current_task()->parent_group->file_open, stdin);  // stdin
    stdout->fd = queue_enqueue(get_current_task()->parent_group->file_open, stdout); // stdout
    stderr->fd = queue_enqueue(get_current_task()->parent_group->file_open, stderr); // stderr

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp),
                       "r"(get_current_task()->context0.rflags), "r"((uint64_t)0x23),
                       "r"(main_func), "r"((uint64_t)0x1b)
                     : "memory");
}

void kill_proc(pcb_t pcb, int exit_code) {
    if (pcb == NULL) return;
    if (pcb->pgb_id == kernel_group->pgb_id) {
        kerror("Cannot kill System process.");
        return;
    }
    close_interrupt;
    disable_scheduler();
    pcb->status       = DEATH;
    ipc_message_t msg = malloc(sizeof(struct ipc_message));
    msg->pid          = pcb->pgb_id;
    msg->type         = IPC_MSG_TYPE_EPID;
    msg->data[0]      = exit_code & 0xFF;
    msg->data[1]      = (exit_code >> 8) & 0xFF;
    msg->data[2]      = (exit_code >> 16) & 0xFF;
    msg->data[3]      = (exit_code >> 24) & 0xFF;
    ipc_send(pcb->parent_task, msg);
    queue_foreach(pcb->pcb_queue, node) {
        tcb_t tcb = (tcb_t)node->data;
        kill_thread(tcb);
    }
    add_death_proc(pcb);
    enable_scheduler();
    open_interrupt;
}

void kill_proc0(pcb_t pcb) {
    close_interrupt;
    disable_scheduler();

    queue_destroy(pcb->pcb_queue);
    queue_remove_at(pgb_queue, pcb->queue_index);

    loop {
        fd_file_handle *handle = (fd_file_handle *)queue_dequeue(pcb->file_open);
        if (handle == NULL) break;
        vfs_close(handle->node);
        free(handle);
    }

    if (pcb->virt_queue->size > 0) {
    refree_virt:
        mm_virtual_page_t *virt_page = NULL;

        queue_foreach(pcb->virt_queue, node) {
            mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
            virt_page                = vpage;
            break;
        }
        if (virt_page != NULL) {
            queue_remove_at(pcb->virt_queue, virt_page->index);
            free(virt_page);
            goto refree_virt;
        }
    }

    queue_destroy(pcb->file_open);
    queue_destroy(pcb->ipc_queue);
    queue_destroy(pcb->virt_queue);
    free(pcb->cmdline);
    free(pcb->cwd);
    free_tty(pcb->tty);
    free(pcb->elf_file);
    logkf("Freeing process %s (PID: %d) vfork: %s\n", pcb->name, pcb->pgb_id,
          pcb->vfork ? "true" : "false");
    if (!pcb->vfork) free_page_directory(pcb->page_dir);
    free(pcb);
    open_interrupt;
    enable_scheduler();
}

void kill_thread(tcb_t task) {
    if (task == NULL) return;
    if (task->task_level == TASK_KERNEL_LEVEL) {
        kerror("Cannot stop kernel thread.");
        return;
    }
    task->status = DEATH;

    if (task->tid_directory != NULL) {
        page_directory_t *directory = get_current_directory();
        switch_process_page_directory(task->tid_directory);
        futex_wake((void *)page_virt_to_phys(task->tid_address), 1);
        switch_process_page_directory(directory);
    }

    smp_cpu_t *cpu    = get_cpu_smp(task->cpu_id);
    task->death_index = lock_queue_enqueue(cpu->death_queue, task);
    futex_free(task);
    spin_unlock(cpu->death_queue->lock);
}

void kill_thread0(tcb_t task) {
    if (task->time_buf != NULL) free(task->time_buf);
    task->status              = OUT;
    page_directory_t *src_dir = get_current_directory();
    switch_process_page_directory(task->parent_group->page_dir);
    int *tid_addr = (int *)task->tid_address;
    if (tid_addr != NULL) *tid_addr = 0;
    switch_process_page_directory(src_dir);
    remove_task(task);
}

pcb_t found_pcb(int pid) {
    queue_foreach(pgb_queue, node) {
        pcb_t pcb = (pcb_t)node->data;
        if (pcb->pgb_id == pid) { return pcb; }
    }
    return NULL;
}

tcb_t found_thread(pcb_t pcb, int tid) {
    if (pcb == NULL) return NULL;
    queue_foreach(pcb->pcb_queue, node) {
        tcb_t thread = (tcb_t)node->data;
        if (thread->pid == tid) { return thread; }
    }
    return NULL;
}

int waitpid(int pid) {
    if (found_pcb(pid) == NULL) return -25565;
    ipc_message_t mesg;
    bool          is_sti = are_interrupts_enabled();
    loop {
        open_interrupt;
        mesg = ipc_recv_wait(IPC_MSG_TYPE_EPID);
        if (pid == mesg->pid) {
            int exit_code = (mesg->data[3] << 24) | (mesg->data[2] << 16) | (mesg->data[1] << 8) |
                            mesg->data[0];
            free(mesg);
            if (!is_sti) close_interrupt;
            return exit_code;
        }
        ipc_send(get_current_task()->parent_group, mesg);
    }
}

void kill_all_proc() {
    close_interrupt;
    disable_scheduler();
    lapic_timer_stop();
}

pcb_t create_process_group(char *name, page_directory_t *directory, ucb_t user_handle,
                           char *cmdline, pcb_t parent_process, void *elf_file, size_t elf_size) {
    pcb_t new_pgb = malloc(sizeof(struct process_control_block));
    memset(new_pgb, 0, sizeof(struct process_control_block));
    new_pgb->pgb_id = now_pid++;
    strcpy(new_pgb->name, name);
    new_pgb->parent_task = parent_process == NULL ? kernel_group : parent_process;
    new_pgb->pcb_queue   = queue_init();
    new_pgb->ipc_queue   = queue_init();
    new_pgb->file_open   = queue_init();
    new_pgb->virt_queue  = queue_init();
    new_pgb->tty         = alloc_default_tty();
    new_pgb->task_level  = TASK_KERNEL_LEVEL;
    new_pgb->cmdline     = malloc(strlen(cmdline));
    new_pgb->elf_file    = elf_file;
    new_pgb->elf_size    = elf_size;
    strcpy(new_pgb->cmdline, cmdline);
    new_pgb->user        = user_handle == NULL ? get_kernel_user() : user_handle;
    new_pgb->page_dir    = directory == NULL ? get_kernel_pagedir() : directory;
    new_pgb->parent_task = get_current_task()->parent_group;
    new_pgb->queue_index = lock_queue_enqueue(pgb_queue, new_pgb);
    new_pgb->vfork       = false;
    new_pgb->cwd         = malloc(1024);
    memset(new_pgb->cwd, 0, 1024);
    strcpy(new_pgb->cwd, new_pgb->parent_task->cwd);
    new_pgb->mmap_start = USER_MMAP_START;
    spin_unlock(pgb_queue->lock);
    new_pgb->status = START;
    return new_pgb;
}

int create_user_thread(void (*_start)(void), char *name, pcb_t pcb) {
    if (pcb == NULL || name == NULL || _start == NULL) return -1;

    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)malloc(STACK_SIZE);
    not_null_assets(new_task, "create user task null");
    memset(new_task, 0, sizeof(struct thread_control_block));

    if (pcb == NULL) {
        new_task->group_index = lock_queue_enqueue(kernel_group->pcb_queue, new_task);
        spin_unlock(kernel_group->pcb_queue->lock);
        new_task->pid          = now_tid++;
        new_task->parent_group = kernel_group;
    } else {
        new_task->group_index = lock_queue_enqueue(pcb->pcb_queue, new_task);
        spin_unlock(pcb->pcb_queue->lock);
        new_task->pid          = now_tid++;
        new_task->parent_group = pcb;
    }

    new_task->task_level = TASK_APPLICATION_LEVEL;
    new_task->cpu_clock  = 0;
    new_task->cpu_timer  = 0;
    new_task->mem_usage  = get_all_memusage();
    new_task->cpu_id     = cpu->id;
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top       = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    *(--stack_top)            = (uint64_t)_start;
    *(--stack_top)            = (uint64_t)process_exit;
    *(--stack_top)            = (uint64_t)switch_to_user_mode;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)switch_to_user_mode;
    new_task->context0.rsp = (uint64_t)new_task + STACK_SIZE - sizeof(uint64_t) * 3; // 设置上下文
    new_task->kernel_stack = (new_task->context0.rsp &= ~0xF);                       // 栈16字节对齐

    new_task->user_stack =
        page_alloc_random(pcb->page_dir, BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    new_task->user_stack_top = new_task->user_stack + BIG_USER_STACK;
    new_task->main           = (uint64_t)_start;
    new_task->context0.cs    = 0x8;
    new_task->context0.ss = new_task->context0.es = new_task->context0.ds = 0x10;
    new_task->status                                                      = CREATE;

    new_task->fs_base = (uint64_t)new_task;

    add_task(new_task);
    enable_scheduler();
    open_interrupt;
    return new_task->pid;
}

int create_kernel_thread(int (*_start)(void *arg), void *args, char *name, pcb_t pcb) {
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)malloc(KERNEL_ST_SZ);
    not_null_assets(new_task, "create kernel task null");
    memset(new_task, 0, sizeof(struct thread_control_block));

    if (pcb == NULL) {
        new_task->group_index  = queue_enqueue(kernel_group->pcb_queue, new_task);
        new_task->pid          = now_tid++;
        new_task->parent_group = kernel_group;
    } else {
        queue_enqueue(pcb->pcb_queue, new_task);
        new_task->pid          = now_tid++;
        new_task->parent_group = pcb;
    }

    new_task->task_level = TASK_KERNEL_LEVEL;
    new_task->cpu_clock  = 0;
    new_task->cpu_timer  = 0;
    new_task->mem_usage  = get_all_memusage();
    new_task->cpu_id     = cpu->id;
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top       = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    *(--stack_top)            = (uint64_t)args;
    *(--stack_top)            = (uint64_t)process_exit;
    *(--stack_top)            = (uint64_t)_start;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)_start;
    new_task->context0.rsp = (uint64_t)new_task + STACK_SIZE - sizeof(uint64_t) * 3; // 设置上下文
    new_task->kernel_stack = (new_task->context0.rsp &= ~(uint64_t)0xF);             // 栈16字节对齐
    new_task->user_stack =
        new_task->kernel_stack; // 内核级线程没有用户态的部分, 所以用户栈句柄与内核栈句柄统一
    new_task->user_stack_top = new_task->user_stack;
    new_task->context0.cs    = 0x8;
    new_task->context0.ss    = 0x10;
    new_task->context0.es    = 0x10;
    new_task->context0.ds    = 0x10;
    new_task->status         = CREATE;

    new_task->fs_base = (uint64_t)new_task;

    add_task(new_task);
    enable_scheduler();
    open_interrupt;
    return new_task->pid;
}

void init_pcb() {
    pcb_lock       = SPIN_INIT;
    scheduler_lock = SPIN_INIT;
    pgb_queue      = queue_init();

    kernel_group = malloc(sizeof(struct process_control_block));
    memset(kernel_group, 0, sizeof(struct process_control_block));
    strcpy(kernel_group->name, "System");
    kernel_group->pgb_id      = now_pid++;
    kernel_group->pcb_queue   = queue_init();
    kernel_group->queue_index = queue_enqueue(pgb_queue, kernel_group);
    kernel_group->page_dir    = get_kernel_pagedir();
    kernel_group->user        = get_kernel_user();
    kernel_group->tty         = get_default_tty();
    kernel_group->status      = RUNNING;
    kernel_group->ipc_queue   = queue_init();
    kernel_group->parent_task = kernel_group;
    kernel_group->task_level  = TASK_KERNEL_LEVEL;
    kernel_group->virt_queue  = queue_init();
    kernel_group->cwd         = malloc(1024);
    memset(kernel_group->cwd, 0, 1024);
    kernel_group->cwd[0] = '/';

    kernel_head_task               = (tcb_t)malloc(STACK_SIZE);
    kernel_head_task->parent_group = kernel_group;
    kernel_head_task->task_level   = 0;
    kernel_head_task->pid          = now_tid++;
    kernel_head_task->cpu_clock    = 0;
    set_kernel_stack(get_rsp()); // 给IDLE线程设置TSS内核栈, 不然这个线程炸了后会发生 DoubleFault
    kernel_head_task->kernel_stack = kernel_head_task->context0.rsp = get_rsp();
    kernel_head_task->user_stack      = kernel_head_task->kernel_stack;
    kernel_head_task->user_stack_top  = kernel_head_task->user_stack;
    kernel_head_task->context0.rflags = get_rflags();
    kernel_head_task->cpu_timer       = nano_time();
    kernel_head_task->time_buf        = alloc_timer();
    kernel_head_task->cpu_id          = cpu->id;
    kernel_head_task->status          = RUNNING;

    char name[50];
    sprintf(name, "CP_IDLE_CPU%u", cpu->id);
    memcpy(kernel_head_task->name, name, strlen(name));
    kernel_head_task->name[strlen(name)] = '\0';
    kernel_head_task->group_index        = queue_enqueue(kernel_group->pcb_queue, kernel_head_task);
    futex_init();
    kinfo("Load task schedule. | Process(%s) PID: %d", kernel_group->name, kernel_head_task->pid);
}
