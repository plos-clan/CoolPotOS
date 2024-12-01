#include "pcb.h"
#include "kmalloc.h"
#include "klog.h"
#include "tty.h"
#include "scheduler.h"
#include "krlibc.h"
#include "io.h"
#include "description_table.h"
#include "elf_util.h"
#include "free_page.h"

extern pcb_t *current_pcb;
extern pcb_t *running_proc_head;
// scheduler.c

int now_pid;

extern void *program_break;
extern void *program_break_end;
extern page_directory_t *kernel_directory;
extern tty_t default_tty;

static void add_task(pcb_t *new_task){ //添加进程至调度链
    if(new_task == NULL) return;
    pcb_t *tailt = running_proc_head;
    while (tailt->next != running_proc_head) {
        if(tailt->next == NULL) break;
        tailt = tailt->next;
    }
    tailt->next = new_task;
}

static void switch_to_user_mode(uint32_t func) {
    io_cli();
    uint32_t esp = (uint32_t )get_current_proc()->user_stack + STACK_SIZE;
    get_current_proc()->context.eflags = (0 << 12 | 0b10 | 1 << 9);
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
    __asm__ volatile("movl %0, %%esp\n"
                 "popa\n"
                 "pop %%gs\n"
                 "pop %%fs\n"
                 "pop %%es\n"
                 "pop %%ds\n"
                 "iret" ::"m"(a));
}

_Noreturn static void process_exit(){
    register uint32_t eax __asm__("eax");
    printk("Kernel Process exit, Code: %d\n",eax);
    kill_proc(current_pcb);
    while (1);
}

int create_user_process(const char* path,const char* cmdline,char* name,uint8_t level){
    vfs_node_t exefile = vfs_open(path);
    if(exefile == NULL) return -1;

    pcb_t *new_task = kmalloc(STACK_SIZE);
    memset(new_task, 0, sizeof(pcb_t));

    new_task->task_level = level;
    strcpy(new_task->name,name);
    strcpy(new_task->cmdline,cmdline);
    new_task->pid = now_pid++;
    new_task->pgd_dir = clone_directory(kernel_directory);
    new_task->cpu_clock = 0;
    new_task->tty = default_tty_alloc();
    new_task->cpu_clock = 0;
    new_task->exe_file = exefile;
    new_task->kernel_stack = new_task;
    new_task->sche_time = 1;
    new_task->program_break = new_task->program_break_end = (void*)USER_AREA_START;

    // 映射形参数据区
    new_task->program_break_end += PAGE_SIZE;
    for (uint32_t i = (uint32_t)new_task->program_break; i < (uint32_t)new_task->program_break_end; i += PAGE_SIZE) {
        alloc_frame(get_page(i,1,new_task->pgd_dir),0,1);
    }
    new_task->program_break += PAGE_SIZE;

    //映射用户堆初始大小
    new_task->program_break_end += USER_AREA_SIZE;
    for (uint32_t i = (uint32_t)new_task->program_break; i < (uint32_t)new_task->program_break_end; i += PAGE_SIZE) {
        alloc_frame(get_page(i,1,new_task->pgd_dir),0,1);
    }

    // 映射用户栈区
    for (uint32_t i = USER_STACK_TOP - STACK_SIZE; i < USER_STACK_TOP; i+= PAGE_SIZE) {
        alloc_frame(get_page(i,1,new_task->pgd_dir),0,1);
    }
    new_task->user_stack = (void*)0xb2000000 - STACK_SIZE;

    // 读取可执行文件
    page_directory_t *cur_dir = get_current_proc()->pgd_dir;
    disable_scheduler();
    io_sti();
    switch_page_directory(new_task->pgd_dir);
    uint8_t *data = kmalloc(exefile->size);
    if(vfs_read(exefile,data,0,exefile->size) == -1){
        vfs_close(exefile);
        return -1;
    }
    uint32_t _start = elf_load(exefile->size,data);
    kfree(data);
    switch_page_directory(cur_dir);
    io_cli();
    enable_scheduler();

    if(_start == 0){
        free_tty(new_task->tty);
        kfree(new_task);
        vfs_close(exefile);
        return -1;
    }

    uint32_t *stack_top = (uint32_t * )((uint32_t) new_task + STACK_SIZE);
    *(--stack_top) = (uint32_t) _start;
    *(--stack_top) = (uint32_t) process_exit;
    *(--stack_top) = (uint32_t) switch_to_user_mode;
    new_task->context.esp = (uint32_t) new_task + STACK_SIZE - sizeof(uint32_t) * 3;
    new_task->context.eflags = 0x200;

    add_task(new_task);
    io_sti();
    return new_task->pid;
}


int create_kernel_thread(int (*_start)(void* arg),void *args,char* name){ //创建内核进程 (内核线程)
    io_cli();
    pcb_t *new_task = kmalloc(STACK_SIZE);
    memset(new_task, 0, sizeof(pcb_t));

    new_task->task_level = TASK_KERNEL_LEVEL;
    strcpy(new_task->name,name);
    new_task->pid = now_pid++;
    new_task->program_break = program_break;
    new_task->program_break_end = program_break_end;
    new_task->pgd_dir = kernel_directory;
    new_task->cpu_clock = 0;
    new_task->tty = default_tty_alloc();
    new_task->cpu_clock = 0;
    new_task->sche_time = 1;
    new_task->user_stack = new_task->kernel_stack;

    new_task->kernel_stack = new_task;

    uint32_t *stack_top = (uint32_t * )((uint32_t) new_task + STACK_SIZE);
    *(--stack_top) = (uint32_t) args;
    *(--stack_top) = (uint32_t) process_exit;
    *(--stack_top) = (uint32_t) _start;

    new_task->context.esp = (uint32_t) new_task + STACK_SIZE - sizeof(uint32_t) * 3;
    new_task->context.eflags = 0x200;

    add_task(new_task);
    io_sti();
    return new_task->pid;
}

void kill_all_proc() {
    pcb_t *head = running_proc_head;
    while (1) {
        head = head->next;
        if (head == NULL || head->pid == running_proc_head->pid) {
            return;
        }
        if (head->pid == get_current_proc()->pid) continue;
        printk("Kill process [PID: %d].\n",head->pid);
        kill_proc(head);
    }
}

void kill_proc(pcb_t *pcb){
    io_cli();
    if (pcb->pid == 0) {
        klogf(false,"Cannot kill kernel processes.\n");
        io_sti();
        return;
    }

    if(pcb->task_level == TASK_KERNEL_LEVEL){

    } else{
        vfs_close(pcb->exe_file);
        put_directory(pcb->pgd_dir);
    }

    free_tty(pcb->tty);

    pcb_t *head = running_proc_head;
    pcb_t *last = NULL;
    while (1) {
        if (head->pid == pcb->pid) {
            last->next = pcb->next;
            kfree(pcb);
            io_sti();
            return;
        }
        last = head;
        head = head->next;
    }
}

pcb_t *found_pcb(int pid){
    pcb_t *l = running_proc_head;
    while (l->pid != pid){
        l = l->next;
        if(l == NULL || l == running_proc_head) return NULL;
    }
    return l;
}

void init_pcb(){
    current_pcb = kmalloc(sizeof(pcb_t));

    current_pcb->task_level = TASK_KERNEL_LEVEL;
    current_pcb->pid = now_pid++;
    strcpy(current_pcb->name,"CP_IDLE");
    current_pcb->next = current_pcb;
    current_pcb->kernel_stack = current_pcb;
    current_pcb->tty = &default_tty;
    current_pcb->sche_time = 1;
    current_pcb->pgd_dir = kernel_directory;
    current_pcb->context.esp = (uint32_t )current_pcb->kernel_stack;

    current_pcb->program_break = program_break;
    current_pcb->program_break_end = program_break_end;

    running_proc_head = current_pcb;
    klogf(true,"Load task schedule. | KernelProcessName: %s PID: %d\n",current_pcb->name,current_pcb->pid);
}
