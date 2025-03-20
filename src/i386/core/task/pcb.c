#include "pcb.h"
#include "description_table.h"
#include "elf_util.h"
#include "free_page.h"
#include "io.h"
#include "klog.h"
#include "kmalloc.h"
#include "krlibc.h"
#include "pipfs.h"
#include "scheduler.h"
#include "tcb.h"
#include "tty.h"

extern pcb_t *current_pcb;
extern pcb_t *running_proc_head;
// scheduler.c

int now_pid;

extern void             *program_break;
extern void             *program_break_end;
extern page_directory_t *kernel_directory;
extern tty_t             default_tty;

static void add_task(pcb_t *new_task) { // 添加进程至调度链
    if (new_task == NULL) return;       // 参数检查
    pcb_t *tailt = running_proc_head;
    while (tailt->next != running_proc_head) { // 查找调度链尾部节点
        if (tailt->next == NULL) break;
        tailt = tailt->next;
    }
    tailt->next = new_task; // 添加新进程
    pipfs_update();         // 更新调度信息
}

void switch_to_user_mode(uint32_t func) {
    io_cli(); // 关闭中断，避免切换过程被打断
    uint32_t esp = (uint32_t)get_current_proc()->user_stack + STACK_SIZE; // 设置用户态堆栈指针
    get_current_proc()->context.eflags = (0 << 12 | 0b10 | 1 << 9);       // 设置寄存器
    intr_frame_t iframe;                                                  // 初始化中断帧
    iframe.edi       = 1;
    iframe.esi       = 2;
    iframe.ebp       = 3;
    iframe.esp_dummy = 4;
    iframe.ebx       = 5;
    iframe.edx       = 6;
    iframe.ecx       = 7;
    iframe.eax       = 8;
    iframe.gs        = GET_SEL(4 * 8, SA_RPL3);
    iframe.ds        = GET_SEL(4 * 8, SA_RPL3);
    iframe.es        = GET_SEL(4 * 8, SA_RPL3);
    iframe.fs        = GET_SEL(4 * 8, SA_RPL3);
    iframe.ss        = GET_SEL(4 * 8, SA_RPL3);
    iframe.cs        = GET_SEL(3 * 8, SA_RPL3);
    iframe.eip       = func; // 用户可执行程序入口
    iframe.eflags    = (0 << 12 | 0b10 | 1 << 9);
    iframe.esp       = esp; // 设置用户态堆栈

    intr_frame_t *a = &iframe; // 切换到用户模式
    __asm__ volatile("movl %0, %%esp\n"
                     "popa\n"
                     "pop %%gs\n"
                     "pop %%fs\n"
                     "pop %%es\n"
                     "pop %%ds\n"
                     "iret" ::"m"(a));
}

_Noreturn void process_exit() {
    register uint32_t eax __asm__("eax");           // 获取退出代码
    printk("Kernel Process exit, Code: %d\n", eax); // 打印退出信息
    kill_proc(current_pcb);                         // 终止进程
    while (1)
        ; // 确保进程不被继续执行
}

int create_user_process(const char *path, const char *cmdline, char *name, uint8_t level) {
    vfs_node_t exefile = vfs_open(path); // 打开可执行文件
    if (exefile == NULL) return -1;

    pcb_t *new_task = kmalloc(STACK_SIZE); // 分配和初始化PCB（进程控制块）
    memset(new_task, 0, sizeof(pcb_t));

    new_task->task_level = level;                                // 设置进程优先级
    strcpy(new_task->name, name);                                // 复制进程名称
    strcpy(new_task->cmdline, cmdline);                          // 复制命令行参数
    new_task->pid           = now_pid++;                         // 设置进程pid
    new_task->pgd_dir       = clone_directory(kernel_directory); // 克隆内核页目录
    new_task->cpu_clock     = 0;                                 // 初始化CPU时钟计数器
    new_task->tty           = default_tty_alloc();               // 分配TTY设备
    new_task->exe_file      = exefile;                           // 设置节点指针
    new_task->kernel_stack  = new_task;                          // 设置内核栈指针
    new_task->sche_time     = 1;                                 // 设置调度时间
    new_task->program_break = new_task->program_break_end =
        (void *)USER_AREA_START;  // 初始化程序断点和结束地址
    new_task->now_tid  = 0;       // 初始化当前线程ID
    new_task->fpu_flag = 0;       // 初始化FPU标志
    new_task->status   = RUNNING; // 设置进程状态
    for (int i = 0; i < 255; i++)
        new_task->file_table[i] = NULL; // 初始化文件表
    // 映射形参数据区
    new_task->program_break_end += PAGE_SIZE;
    for (uint32_t i  = (uint32_t)new_task->program_break; i < (uint32_t)new_task->program_break_end;
         i          += PAGE_SIZE) {
        alloc_frame(get_page(i, 1, new_task->pgd_dir), 0, 1);
    }
    new_task->program_break += PAGE_SIZE;

    // 映射用户堆初始大小
    new_task->program_break_end += USER_AREA_SIZE;
    for (uint32_t i  = (uint32_t)new_task->program_break; i < (uint32_t)new_task->program_break_end;
         i          += PAGE_SIZE) {
        alloc_frame(get_page(i, 1, new_task->pgd_dir), 0, 1);
    }

    for (uint32_t i  = USER_STACK_TOP - STACK_SIZE; i < USER_STACK_TOP;
         i          += PAGE_SIZE) { // 映射用户栈区
        alloc_frame(get_page(i, 1, new_task->pgd_dir), 0, 1);
    }
    new_task->user_stack = (void *)USER_STACK_TOP - STACK_SIZE;

    page_directory_t *cur_dir = get_current_proc()->pgd_dir; // 读取可执行文件
    disable_scheduler();
    io_sti();
    switch_page_directory(new_task->pgd_dir);

    Elf32_Ehdr *data = kmalloc(exefile->size);
    if (vfs_read(exefile, data, 0, exefile->size) == -1) {
        vfs_close(exefile);
        return -1;
    }
    uint32_t _start = load_elf(data, new_task->pgd_dir);
    new_task->data  = data;
    switch_page_directory(cur_dir);
    io_cli();
    enable_scheduler();

    if (_start == 0) { // 检查加载结果
        free_tty(new_task->tty);
        kfree(new_task);
        vfs_close(exefile);
        return -1;
    }

    create_user_thread(new_task, (void *)_start); // 创建用户线程

    uint32_t *stack_top = (uint32_t *)((uint32_t)new_task->kernel_stack + STACK_SIZE); // 设置内核栈
    *(--stack_top)      = (uint32_t)_start;
    *(--stack_top)      = (uint32_t)process_exit;
    *(--stack_top)      = (uint32_t)switch_to_user_mode;
    new_task->context.esp    = (uint32_t)new_task + STACK_SIZE - sizeof(uint32_t) * 3;
    new_task->context.eflags = 0x200;

    add_task(new_task); // 添加到调度链
    io_sti();
    return new_task->pid;
}

int create_kernel_thread(int (*_start)(void *arg), void *args,
                         char *name) {     // 创建内核进程 (内核线程)
    io_cli();                              // 关闭中断
    pcb_t *new_task = kmalloc(STACK_SIZE); // 分配及初始化PCB
    memset(new_task, 0, sizeof(pcb_t));

    new_task->task_level = TASK_KERNEL_LEVEL; // 设置基本信息
    strcpy(new_task->name, name);
    new_task->pid               = now_pid++;
    new_task->program_break     = program_break;
    new_task->program_break_end = program_break_end;
    new_task->pgd_dir           = kernel_directory;
    new_task->cpu_clock         = 0;
    new_task->tty               = default_tty_alloc();
    new_task->sche_time         = 1;
    new_task->user_stack        = new_task->kernel_stack;
    new_task->data              = NULL;
    new_task->fpu_flag          = 0;
    new_task->kernel_stack      = new_task;
    new_task->status            = RUNNING;
    for (int i = 0; i < 255; i++)
        new_task->file_table[i] = NULL;
    uint32_t *stack_top = (uint32_t *)((uint32_t)new_task + STACK_SIZE); // 设置内核栈
    *(--stack_top)      = (uint32_t)args;
    *(--stack_top)      = (uint32_t)process_exit;
    *(--stack_top)      = (uint32_t)_start;

    new_task->context.esp    = (uint32_t)new_task + STACK_SIZE - sizeof(uint32_t) * 3; // 设置上下文
    new_task->context.eflags = 0x200;

    add_task(new_task); // 添加到调度链
    io_sti();
    return new_task->pid;
}

void kill_all_proc() {
    pcb_t *head = running_proc_head; // 获取头指针
    while (1) {                      // 遍历调度链表
        head = head->next;
        if (head == NULL || head->pid == running_proc_head->pid) { return; }
        if (head->pid == get_current_proc()->pid) continue;
        printk("Kill process [PID: %d].\n", head->pid); // 打印被结束进程pid
        kill_proc(head);                                // 结束进程
    }
}

void kill_proc(pcb_t *pcb) {
    io_cli();                                            // 关闭中断
    disable_scheduler();                                 // 禁用调度器
    if (pcb->pid == 0) {                                 // 检查进程ID
        klogf(false, "Cannot kill kernel processes.\n"); // 打印错误信息
        io_sti();                                        // 启用中断
        return;
    }

    pcb->status = DEATH; // 设置进程状态

    for (int i = 0; i < 255; i++) { // 清理文件资源
        cfile_t file = pcb->file_table[i];
        if (file != NULL) { vfs_close(file->handle); }
    }

    if (pcb->task_level == TASK_KERNEL_LEVEL) { // 判断是否为内核进程

    } else {
        kfree(pcb->data);            // 释放数据区
        vfs_close(pcb->exe_file);    // 关闭可执行文件
        put_directory(pcb->pgd_dir); // 释放页目录
    }

    free_tty(pcb->tty); // 释放TTY设备
    pipfs_update();     // 更新调度信息

    pcb_t *head = running_proc_head;
    pcb_t *last = NULL;
    while (1) { // 从调度链表中移除进程
        if (head->pid == pcb->pid) {
            last->next = pcb->next;
            kfree(pcb);
            enable_scheduler();
            io_sti();
            return;
        }
        last = head;
        head = head->next;
    }
}

pcb_t *found_pcb(int pid) {
    pcb_t *l = running_proc_head; // 获取头指针
    while (l->pid != pid) {       // 查找进程
        l = l->next;
        if (l == NULL || l == running_proc_head) return NULL;
    }
    return l; // 返回PCB指针
}

void init_pcb() {
    current_pcb = kmalloc(sizeof(pcb_t)); // 分配PCB

    current_pcb->task_level = TASK_KERNEL_LEVEL; // 设置基本信息
    current_pcb->pid        = now_pid++;
    strcpy(current_pcb->name, "CP_IDLE");
    current_pcb->next         = current_pcb;
    current_pcb->kernel_stack = current_pcb;
    current_pcb->tty          = &default_tty;
    current_pcb->sche_time    = 1;
    current_pcb->pgd_dir      = kernel_directory;
    current_pcb->context.esp  = (uint32_t)current_pcb->kernel_stack;
    current_pcb->cpu_clock    = 0;
    for (int i = 0; i < 255; i++)
        current_pcb->file_table[i] = NULL;
    current_pcb->program_break     = program_break;
    current_pcb->program_break_end = program_break_end;

    running_proc_head = current_pcb; // 设置调度链表头指针
    klogf(true, "Load task schedule. | KernelProcessName: %s PID: %d\n", current_pcb->name,
          current_pcb->pid); // 打印初始化信息
}
