#include "pcb.h"
#include "krlibc.h"
#include "alloc.h"
#include "kprint.h"
#include "scheduler.h"
#include "io.h"
#include "pivfs.h"
#include "description_table.h"
#include "timer.h"
#include "heap.h"
#include "sprintf.h"
#include "smp.h"
#include "lock.h"

extern pcb_t kernel_head_task;
ticketlock pcb_lock;

int now_pid = 0;

_Noreturn void process_exit() {
    register uint64_t rax __asm__("rax");
    printk("Kernel Process exit, Code: %d\n", rax);
    kill_proc(get_current_task());
    while (1);
}

void switch_to_user_mode(uint64_t func) {
    close_interrupt;
    uint64_t rsp = (uint64_t) get_current_task()->user_stack + STACK_SIZE;
    get_current_task()->context0.rflags = (0 << 12 | 0b10 | 1 << 9);
    __asm__ volatile(
            "pushq %5\n" // SS
            "pushq %1\n" // RSP
            "pushq %2\n" // RFLAGS
            "pushq %3\n" // CS
            "pushq %4\n" // RIP

            "mov %0, %%gs\n"
            "mov %0, %%fs\n"
            "mov %0, %%es\n"
            "mov %0, %%ds\n"
            "iretq\n"
    :
    :"r" ((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp), "r"(get_current_task()->context0.rflags),
    "r"((uint64_t)0x23),"r"(func),"r"((uint64_t)0x1b)
    :"memory");
}

void kill_proc(pcb_t task){
    if(task->task_level == TASK_KERNEL_LEVEL){
        kerror("Cannot stop kernel thread.");
        return;
    }
    ticket_lock(&pcb_lock);
    close_interrupt;
    disable_scheduler(); // 终止调度器, 防止释放被杀进程时候调度到该进程发生错误
    free_tty(task->tty);

    remove_task(task);

    enable_scheduler();
    open_interrupt;
    ticket_unlock(&pcb_lock);
}

pcb_t found_pcb(int pid) {
    return NULL;
}

void kill_all_proc() {
    close_interrupt;
    disable_scheduler();
    lapic_timer_stop();
}

int create_kernel_thread(int (*_start)(void *arg), void *args, char *name){
    pcb_t new_task = (pcb_t)malloc(STACK_SIZE);
    memset(new_task,0, sizeof(struct process_control_block));

    new_task->task_level = 0;
    new_task->pid = now_pid++;
    new_task->cpu_clock = 0;
    new_task->cpu_timer = 0;
    new_task->mem_usage = get_all_memusage();
    new_task->tty = alloc_default_tty(); // 初始化TTY设备
    new_task->directory = get_kernel_pagedir();
    new_task->cpu_id = get_current_cpuid();
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top = (uint64_t *) ((uint64_t) new_task + STACK_SIZE);
    *(--stack_top) = (uint64_t) args;
    *(--stack_top) = (uint64_t) process_exit;
    *(--stack_top) = (uint64_t) _start;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip = (uint64_t)_start;
    new_task->context0.rsp = (uint64_t) new_task + STACK_SIZE - sizeof(uint64_t) * 3; //设置上下文
    new_task->kernel_stack = (new_task->context0.rsp &= ~0xF); // 栈16字节对齐
    new_task->user_stack = new_task->kernel_stack; //内核级进程没有用户态的部分, 所以用户栈句柄与内核栈句柄统一
    add_task(new_task);
    return new_task->pid;
}

void init_pcb(){
    kernel_head_task = (pcb_t)malloc(STACK_SIZE);
    kernel_head_task->task_level = 0;
    kernel_head_task->pid = now_pid++;
    kernel_head_task->cpu_clock = 0;
    kernel_head_task->directory = get_kernel_pagedir();
    set_kernel_stack(get_rsp()); //给IDLE进程设置TSS内核栈, 不然这个进程炸了后会发生 DoubleFault
    kernel_head_task->kernel_stack = kernel_head_task->context0.rsp = get_rsp();
    kernel_head_task->user_stack = kernel_head_task->kernel_stack;
    kernel_head_task->tty = get_default_tty();
    kernel_head_task->context0.rflags = get_rflags();
    kernel_head_task->cpu_timer = nanoTime();
    kernel_head_task->time_buf = alloc_timer();
    kernel_head_task->cpu_id = get_current_cpuid();
    char name[50];
    sprintf(name,"CP_IDLE_CPU%lu",get_current_cpuid());
    memcpy(kernel_head_task->name, name, strlen(name));
    kernel_head_task->name[strlen(name)] = '\0';
    pivfs_update(kernel_head_task);

    kinfo("Load task schedule. | KernelProcess PID: %d", kernel_head_task->pid);
}
