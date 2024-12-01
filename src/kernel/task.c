#include "../include/task.h"
#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/description_table.h"
#include "../include/vfs.h"
#include "../include/timer.h"
#include "../include/shell.h"
#include "../include/heap.h"
#include "../include/elf.h"

#define SA_RPL_MASK 0xFFFC
#define SA_TI_MASK 0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

struct task_struct *running_proc_head = NULL;
struct task_struct *wait_proc_head = NULL;
struct task_struct *current = NULL;

extern page_directory_t *kernel_directory;

extern void switch_to(struct context *prev, struct context *next);

extern void taskX_switch(struct context *prev, struct context *next);

int now_pid = 0;
int can_sche = 1;

struct task_struct *get_current() {
    return current;
}

static uint32_t padding_up(uint32_t num, uint32_t size) {
    return (num + size - 1) / size;
}

void print_proc_t(int *i, struct task_struct *base, struct task_struct *cur, int is_print) {
    if (cur->pid == base->pid) {
        if (is_print) {
            switch (cur->state) {
                case TASK_RUNNABLE:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Running");
                    break;
                case TASK_SLEEPING:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Sleeping");
                    break;
                case TASK_UNINIT:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Init");
                    break;
                case TASK_ZOMBIE:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Zombie");
                    break;
                case TASK_DEATH:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Death");
                    break;
            }
        }
        (*i)++;
    } else {
        if (is_print) {
            switch (cur->state) {
                case TASK_RUNNABLE:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Running");
                    break;
                case TASK_SLEEPING:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Sleeping");
                    break;
                case TASK_UNINIT:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Init");
                    break;
                case TASK_ZOMBIE:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Zombie");
                    break;
                case TASK_DEATH:
                    printf("%-17s      %-2d     %s   %d\n", cur->name, cur->pid, "Death");
                    break;
            }
        }
        (*i)++;
        print_proc_t(i, base, cur->next, is_print);
    }
}

int get_procs() {
    int index = 0;
    print_proc_t(&index, current, current->next, 0);
    return index;
}

void print_proc() {
    int index = 0;
    print_proc_t(&index, current, current->next, 1);
    printf("====---------------[Processes]----------------====\n");
    printf("Name                  Pid     Status    MemUsage [All Proc: %d]\n\n", index);
}

static void
found_task(int pid, struct task_struct *head, struct task_struct *base, struct task_struct **argv, int first) {
    struct task_struct *t = base;
    if (t == NULL) {
        argv = NULL;
        return;
    }
    if (t->pid == pid) {
        *argv = t;
        return;
    } else {
        if (!first)
            if (head->pid == t->pid) {
                argv = NULL;
                return;
            }
        found_task(pid, head, t->next, argv, 0);
    }
}

struct task_struct *found_task_pid(int pid) {
    struct task_struct *argv = NULL;
    found_task(pid, running_proc_head, running_proc_head, &argv, 1);
    if (argv == NULL) {
        printf("Cannot found task Pid:[%d].\n", pid);
        return NULL;
    }
    return argv;
}

void wait_task(struct task_struct *task) {
    task->state = TASK_SLEEPING;
}

void start_task(struct task_struct *task) {
    task->state = TASK_RUNNABLE;
}

void task_kill(int pid) {
    io_cli();
    struct task_struct *argv = found_task_pid(pid);
    if (argv == NULL) {
        printf("Cannot found task Pid:[%d].\n", pid);
        io_sti();
        return;
    }
    if (argv->pid == 0) {
        printf("\033ff3030;[kernel]: Taskkill cannot terminate kernel processes.\033c6c6c6;\n");
        io_sti();
        return;
    }
    argv->state = TASK_DEATH;
    logkf("Taskkill process PID:%d Name:%s\n", argv->pid, argv->name);
    free_tty(argv);
    kfree(argv->name);
    kfree(argv->argv);
    struct task_struct *head = running_proc_head;
    struct task_struct *last = NULL;
    while (1) {
        if (head->pid == argv->pid) {
            last->next = argv->next;
            kfree(argv);
            io_sti();
            return;
        }
        last = head;
        head = head->next;
    }
}

void schedule(registers_t *reg) {
    io_cli();
    if (current && can_sche) {
        current->cpu_clock++;
        change_task_to(reg,current->next);
    }
}

void change_task_to(registers_t *reg,struct task_struct *next) {
    if (current != next) {
        struct task_struct *prev = current;
        current = next;
        page_switch(current->pgd_dir);
        set_kernel_stack(current->stack + STACK_SIZE);


        /*
        if(current->fpu_flag) {
            set_cr0(get_cr0() & ~((1 << 2) | (1 << 3)));
            asm volatile("fnsave (%%eax) \n" ::"a"(&(current->context.fpu_regs)) : "memory");
            set_cr0(get_cr0() | (1 << 2) | (1 << 3));
        }
         */

        prev->context.eip = reg->eip;
        prev->context.ds = reg->ds;
        prev->context.cs = reg->cs;
        prev->context.eax = reg->eax;
        prev->context.ss = reg->ss;
        switch_to(&(prev->context), &(current->context));
        reg->ds = current->context.ds;
        reg->cs = current->context.cs;
        reg->eip = current->context.eip;
        reg->eax = current->context.eax;
        reg->ss = current->context.ss;
    }
}

int32_t user_process(char *path, char *name,char* argv,uint8_t level){ // 用户进程创建
    can_sche = 0;
    if(path == NULL){
        return NULL;
    }
    io_sti();

    uint32_t size = vfs_filesize(path);

    if(size == -1){
        can_sche = 1;
        return NULL;
    }

    io_cli();

    struct task_struct *new_task = (struct task_struct *) kmalloc(STACK_SIZE);
    assert(new_task != NULL, "user_pcb: kmalloc error");

    // 将栈低端结构信息初始化为 0
    memset(new_task, 0, sizeof(struct task_struct));

    new_task->state = TASK_RUNNABLE;
    new_task->stack = current;
    new_task->pid = now_pid++;

    page_directory_t *page = clone_directory(kernel_directory);
    new_task->pgd_dir = page;
    new_task->mem_size = 0;
    new_task->program_break = USER_START + 0xf0000;
    new_task->program_break_end = USER_HEAP_END;
    new_task->isUser = 1;
    new_task->vfs_now = NULL;
    new_task->tty = kmalloc(sizeof(tty_t));
    new_task->cpu_clock = 0;
    new_task->page_alloc_address = USER_END + 0x1000;
    new_task->task_level = level;
    new_task->fpu_flag = false;
    init_default_tty(new_task);
    io_sti();



    vfs_copy(new_task,get_current()->vfs_now);

    char* ker_path = kmalloc(strlen(path) + 1);
    char* use_arg = kmalloc(strlen(argv) + 1);
    char* k_name = kmalloc(strlen(name) + 1);
    strcpy(k_name,name);
    strcpy(use_arg,argv);
    strcpy(ker_path,path);

    new_task->name = k_name;

    new_task->argv = use_arg;

    page_directory_t *cur_page_dir = get_current()->pgd_dir;
    page_switch(page);


    for (int i = USER_START; i < USER_END + 0x1000;i++) { //用户堆以及用户栈映射
        page_t *pg = get_page(i,1,page, false);
        alloc_frame(pg,0,1);
    }

    for (int i = USER_EXEC_FILE_START; i < USER_EXEC_FILE_START + size; i++) {
        page_t *pg = get_page(i,1,page, false);
        alloc_frame(pg,0,1);
    }

    char* buffer =  USER_EXEC_FILE_START;

    memset(buffer,0,size);
    int r = vfs_readfile(ker_path,buffer);

    Elf32_Ehdr *ehdr = buffer;

    if(!elf32Validate(ehdr)){
        printf("Unknown exec file format.\n");
        kfree(ker_path);
        can_sche = 1;
        return NULL;
    }
    uint32_t main = ehdr->e_entry;
    load_elf(ehdr,page);


    uint32_t *stack_top = (uint32_t * )((uint32_t) new_task + STACK_SIZE); 

    *(--stack_top) = (uint32_t) main;
    //*(--stack_top) = (uint32_t) buffer;
    *(--stack_top) = (uint32_t) kthread_exit;
    *(--stack_top) = (uint32_t) switch_to_user_mode;

    new_task->context.esp = (uint32_t) new_task + STACK_SIZE - sizeof(uint32_t) * 3;

    // 设置新任务的标志寄存器未屏蔽中断，很重要
    new_task->context.eflags = (0 << 12 | 0b10 | 1 << 9);
    new_task->next = running_proc_head;
    kfree(ker_path);

    page_switch(cur_page_dir);
    can_sche = 1;

    // 找到当前进任务队列，插入到末尾
    struct task_struct *tailt = running_proc_head;
    assert(tailt != NULL, "Must init sched!");

    while (tailt->next != running_proc_head) {
        tailt = tailt->next;
    }
    tailt->next = new_task;
    can_sche = 1;
    io_sti();
    return new_task->pid;
}

int32_t kernel_thread(int (*fn)(void *), void *arg, char *name) { // 内核进程(线程) 创建
    io_cli();
    struct task_struct *new_task = (struct task_struct *) kmalloc(STACK_SIZE);
    assert(new_task != NULL, "kern_thread: kmalloc error");

    // 将栈低端结构信息初始化为 0
    memset(new_task, 0, sizeof(struct task_struct));

    new_task->state = TASK_RUNNABLE;
    new_task->stack = current;
    new_task->pid = now_pid++;
    new_task->pgd_dir = kernel_directory;
    new_task->mem_size = 0;
    new_task->isUser = 0;
    new_task->cpu_clock = 0;
    new_task->page_alloc_address = USER_END + 0x1000;
    new_task->task_level = TASK_KERNEL_LEVEL;
    new_task->fpu_flag = false;

    extern header_t *head;
    extern header_t *tail;
    extern void *program_break;
    extern void *program_break_end;
    new_task->head = head;
    new_task->tail = tail;
    new_task->program_break = program_break;
    new_task->program_break_end = program_break_end;
    new_task->name = name;
    new_task->vfs_now = current->vfs_now;

    new_task->tty = kmalloc(sizeof(tty_t));
    init_default_tty(new_task);

    uint32_t *stack_top = (uint32_t * )((uint32_t) new_task + STACK_SIZE);

    *(--stack_top) = (uint32_t) arg;
    *(--stack_top) = (uint32_t) kthread_exit;
    *(--stack_top) = (uint32_t) fn;

    new_task->context.esp = (uint32_t) new_task + STACK_SIZE - sizeof(uint32_t) * 3;

    // 设置新任务的标志寄存器未屏蔽中断，很重要
    new_task->context.eflags = 0x200;
    new_task->next = running_proc_head;

    // 找到当前进任务队列，插入到末尾
    struct task_struct *tailt = running_proc_head;
    assert(tailt != NULL, "Must init sched!");

    while (tailt->next != running_proc_head) {
        tailt = tailt->next;
    }
    tailt->next = new_task;
    io_sti();
    return new_task->pid;
}

void kthread_exit() {
    register uint32_t val asm ("eax");
    printf("Task [PID: %d] exited with value %d\n", current->pid,val);
    task_kill(current->pid);
    while (1);
}

void kill_all_task() {
    struct task_struct *head = running_proc_head;
    while (1) {
        head = head->next;
        if (head == NULL || head->pid == running_proc_head->pid) {
            return;
        }
        if (head->pid == current->pid) continue;
        task_kill(head->pid);
    }
}

#define SA_RPL3 3

void switch_to_user_mode(uint32_t func) {
    io_cli();
    set_kernel_stack(current->stack + STACK_SIZE);
    unsigned esp = USER_END;
    current->context.eflags = (0 << 12 | 0b10 | 1 << 9);
    intr_frame_t iframe;
    iframe.edi = 1;
    iframe.esi = 2;
    iframe.ebp = 3;
    iframe.esp_dummy = 4;
    iframe.ebx = 5;
    iframe.edx = 6;
    iframe.ecx = 7;
    iframe.eax = 8;
    iframe.gs = GET_SEL(4 * 8, SA_RPL3);
    iframe.ds = GET_SEL(4 * 8, SA_RPL3);
    iframe.es = GET_SEL(4 * 8, SA_RPL3);
    iframe.fs = GET_SEL(4 * 8, SA_RPL3);
    iframe.ss = GET_SEL(4 * 8, SA_RPL3);
    iframe.cs = GET_SEL(3 * 8, SA_RPL3);
    iframe.eip = func; //用户可执行程序入口
    iframe.eflags = (0 << 12 | 0b10 | 1 << 9);
    iframe.esp = esp; // 设置用户态堆栈

    intr_frame_t  *a = &iframe;
    asm volatile("movl %0, %%esp\n"
                 "popa\n"
                 "pop %%gs\n"
                 "pop %%fs\n"
                 "pop %%es\n"
                 "pop %%ds\n"
                 "iret" ::"m"(a));
}

void init_sched() {
    // 为当前执行流创建信息结构体 该结构位于当前执行流的栈最低端
    current = (struct task_struct *) kmalloc(sizeof(struct task_struct));

    current->state = TASK_RUNNABLE;
    current->pid = now_pid++;
    current->stack = current;   // 该成员指向栈低地址
    current->pgd_dir = kernel_directory;
    current->name = "CPOS-System";
    current->mem_size = 0;
    current->next = current;
    current->isUser = 0;

    init_default_tty(current);

    extern header_t *head;
    extern header_t *tail;
    extern void *program_break;
    extern void *program_break_end;
    current->head = head;
    current->tail = tail;
    current->program_break = program_break;
    current->program_break_end = program_break_end;

    running_proc_head = current;
    klogf(true,"Load task schedule. | KernelTaskName: %s PID: %d\n",current->name,current->pid);
}