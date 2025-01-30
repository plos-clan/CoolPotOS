#include "pcb.h"
#include "krlibc.h"
#include "alloc.h"
#include "kprint.h"
#include "scheduler.h"
#include "io.h"
#include "acpi.h"

extern pcb_t current_task;
extern pcb_t kernel_head_task;

static int now_pid = 0;

_Noreturn void process_exit() {
    register uint32_t eax __asm__("eax");//获取退出代码
    printk("Kernel Process exit, Code: %d\n", eax);//打印退出信息
    //kill_proc(current_pcb);//终止进程
    while (1);//确保进程不被继续执行
}

void kill_proc(pcb_t task){
    close_interrupt;
    disable_scheduler();
    free_tty(task->tty);

    pcb_t head = kernel_head_task;
    pcb_t last = NULL;
    while (1) {
        if (head->pid == task->pid) {
            last->next = task->next;
            free(task);
            enable_scheduler();
            open_interrupt;
            return;
        }
        last = head;
        head = head->next;
    }
}

pcb_t found_pcb(int pid) {
    pcb_t l = kernel_head_task;//获取头指针
    while (l->pid != pid) {//查找进程
        l = l->next;
        if (l == NULL || l == kernel_head_task) return NULL;
    }
    return l;//返回PCB指针
}

void kill_all_proc() {
    close_interrupt;
    disable_scheduler();
    lapic_timer_stop();
    pcb_t head = kernel_head_task;//获取头指针
    while (1) {//遍历调度链表
        head = head->next;
        if (head == NULL || head->pid == kernel_head_task->pid) {
            return;
        }
        if (head->pid == get_current_task()->pid) continue;
        kill_proc(head);//结束进程
        disable_scheduler();
    }
}

int create_kernel_thread(int (*_start)(void *arg), void *args, char *name){
    pcb_t new_task = (pcb_t)malloc(STACK_SIZE);
    memset(new_task,0, sizeof(struct process_control_block));

    new_task->task_level = TASK_KERNEL_LEVEL;
    new_task->pid = now_pid++;
    new_task->cpu_clock = 0;
    new_task->tty = alloc_default_tty();
    new_task->directory = get_kernel_pagedir();
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top = (uint64_t *) ((uint64_t) new_task + STACK_SIZE);
    *(--stack_top) = (uint64_t) args;
    *(--stack_top) = (uint64_t) process_exit;
    *(--stack_top) = (uint64_t) _start;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip = (uint64_t)_start;
    new_task->context0.rsp = (uint64_t) new_task + STACK_SIZE - sizeof(uint64_t) * 3;//设置上下文
    new_task->kernel_stack = (new_task->context0.rsp &= ~0xF); // 16字节对齐
    new_task->next = kernel_head_task;
    add_task(new_task);
    return new_task->pid;
}

void init_pcb(){
    kernel_head_task = current_task = (pcb_t)malloc(STACK_SIZE);
    current_task->task_level = TASK_KERNEL_LEVEL;
    current_task->pid = now_pid++;
    current_task->cpu_clock = 0;
    current_task->directory = get_kernel_pagedir();
    current_task->kernel_stack = current_task->context0.rsp = get_rsp();
    current_task->tty = get_default_tty();
    current_task->context0.rflags = get_rflags();
    memcpy(current_task->name, "CP_IDLE\0", 9);
    current_task->next = current_task;
    kinfo("Load task schedule. | KernelProcessName: %s PID: %d", current_task->name, current_task->pid);
}
