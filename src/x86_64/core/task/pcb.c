#include "pcb.h"
#include "alloc.h"
#include "description_table.h"
#include "heap.h"
#include "io.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pivfs.h"
#include "scheduler.h"
#include "smp.h"
#include "sprintf.h"
#include "timer.h"

extern tcb_t      kernel_head_task;
ticketlock        pcb_lock;
extern ticketlock scheduler_lock;

lock_queue *pgb_queue;
pcb_t kernel_group;

int now_pid = 0;

_Noreturn void process_exit() {
    uint64_t rax = 0;
    __asm__("movq %%rax,%0" :: "r"(rax):);
    printk("Kernel Process exit, Code: %d\n", rax);
    kill_proc(get_current_task());
    infinite_loop;
}

void switch_to_user_mode(uint64_t func) {
    close_interrupt;
    uint64_t rsp                        = (uint64_t)get_current_task()->user_stack + STACK_SIZE;
    get_current_task()->context0.rflags = (0 << 12 | 0b10 | 1 << 9);
    __asm__ volatile("pushq %5\n" // SS
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
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp),
                       "r"(get_current_task()->context0.rflags), "r"((uint64_t)0x23), "r"(func),
                       "r"((uint64_t)0x1b)
                     : "memory");
}

void kill_proc(pcb_t pcb){
    queue_foreach(pcb->pcb_queue,node){
        tcb_t tcb = (tcb_t)node->data;
        kill_thread(tcb);
    }
    free_tty(pcb->tty);
    free(pcb);
}

void kill_thread(tcb_t task) {
    if (task->task_level == TASK_KERNEL_LEVEL) {
        kerror("Cannot stop kernel thread.");
        return;
    }
    ticket_lock(&pcb_lock);
    close_interrupt;
    disable_scheduler(); // 终止调度器, 防止释放被杀线程时候调度到该线程发生错误
    free(task->time_buf);

    remove_task(task);

    enable_scheduler();
    open_interrupt;
    ticket_unlock(&pcb_lock);
}

pcb_t found_pcb(int pid){
    queue_foreach(pgb_queue,node){
        pcb_t pcb = (pcb_t)node->data;
        if(pcb->pgb_id == pid) return pcb;
    }
    return NULL;
}

tcb_t found_thread(pcb_t pcb,int tid) {
    if(pcb == NULL) return NULL;
    queue_foreach(pcb->pcb_queue,node){
        tcb_t thread = (tcb_t)node->data;
        if(thread->pid == tid){
            return thread;
        }
    }
    return NULL;
}

void kill_all_proc() {
    close_interrupt;
    disable_scheduler();
    lapic_timer_stop();
}

pcb_t create_process_group(char* name){
    pcb_t new_pgb = malloc(sizeof(struct process_control_block));
    new_pgb->pgb_id = now_pid++;
    strcpy(new_pgb->name,name);
    new_pgb->pcb_queue   = queue_init();
    new_pgb->pid_index   = 0;
    new_pgb->tty         = alloc_default_tty(); //初始化TTY设备
    new_pgb->page_dir    = get_kernel_pagedir();
    new_pgb->queue_index = queue_enqueue(pgb_queue,new_pgb);
    return new_pgb;
}

int create_kernel_thread(int (*_start)(void *arg), void *args, char *name,pcb_t pgb_group) {
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)malloc(KERNEL_ST_SZ);
    memset(new_task, 0, sizeof(struct thread_control_block));

    if(pgb_group == NULL){
        new_task->group_index = queue_enqueue(kernel_group->pcb_queue,new_task);
        new_task->pid = kernel_group->pid_index++;
        new_task->parent_group = kernel_group;
    } else{
        queue_enqueue(pgb_group->pcb_queue,new_task);
        new_task->pid = pgb_group->pid_index++;
        new_task->parent_group = pgb_group;
    }

    new_task->task_level = 0;
    new_task->cpu_clock  = 0;
    new_task->cpu_timer  = 0;
    new_task->mem_usage  = get_all_memusage();
    new_task->cpu_id     = get_current_cpuid();
    memcpy(new_task->name, name, strlen(name) + 1);
    uint64_t *stack_top       = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    *(--stack_top)            = (uint64_t)args;
    *(--stack_top)            = (uint64_t)process_exit;
    *(--stack_top)            = (uint64_t)_start;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)_start;
    new_task->context0.rsp    = (uint64_t)new_task + STACK_SIZE - sizeof(uint64_t) * 3; // 设置上下文
    new_task->kernel_stack    = (new_task->context0.rsp &= ~0xF);                       // 栈16字节对齐
    new_task->user_stack      = new_task->kernel_stack; // 内核级线程没有用户态的部分, 所以用户栈句柄与内核栈句柄统一

    add_task(new_task);
    enable_scheduler();
    open_interrupt;
    return new_task->pid;
}

void init_pcb() {
    ticket_init(&pcb_lock);
    ticket_init(&scheduler_lock);
    pgb_queue = queue_init();

    kernel_group = malloc(sizeof(struct process_control_block));
    strcpy(kernel_group->name,"System");
    kernel_group->pid_index = kernel_group->pgb_id = now_pid++;
    kernel_group->pcb_queue = queue_init();
    kernel_group->queue_index = queue_enqueue(pgb_queue,kernel_group);
    kernel_group->page_dir  = get_kernel_pagedir();
    kernel_group->tty       = get_default_tty();

    kernel_head_task                  = (tcb_t)malloc(STACK_SIZE);
    kernel_head_task->parent_group    = kernel_group;
    kernel_head_task->task_level      = 0;
    kernel_head_task->pid             = kernel_group->pid_index++;
    kernel_head_task->cpu_clock       = 0;
    set_kernel_stack(get_rsp()); // 给IDLE线程设置TSS内核栈, 不然这个线程炸了后会发生 DoubleFault
    kernel_head_task->kernel_stack    = kernel_head_task->context0.rsp = get_rsp();
    kernel_head_task->user_stack      = kernel_head_task->kernel_stack;
    kernel_head_task->context0.rflags = get_rflags();
    kernel_head_task->cpu_timer       = nanoTime();
    kernel_head_task->time_buf        = alloc_timer();
    kernel_head_task->cpu_id          = get_current_cpuid();
    char name[50];
    sprintf(name, "CP_IDLE_CPU%lu", get_current_cpuid());
    memcpy(kernel_head_task->name, name, strlen(name));
    kernel_head_task->name[strlen(name)] = '\0';
    pivfs_update(kernel_head_task);
    kernel_head_task->group_index = queue_enqueue(kernel_group->pcb_queue,kernel_head_task);

    kinfo("Load task schedule. | Process(%s) PID: %d",kernel_group->name, kernel_head_task->pid);
}
