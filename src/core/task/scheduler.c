#include "scheduler.h"
#include "krlibc.h"
#include "pcb.h"
#include "kprint.h"
#include "pivfs.h"
#include "smp.h"
#include "lock.h"
#include "lock_queue.h"

pcb_t current_task = NULL;
pcb_t kernel_head_task = NULL;
bool is_scheduler = false;

ticketlock scheduler_lock;

pcb_t get_current_task(){
    return current_task;
}

void enable_scheduler(){
    is_scheduler = true;
}

void disable_scheduler(){
    is_scheduler = false;
}

int add_task(pcb_t new_task) {
    if (new_task == NULL) return -1;
    ticket_lock(&scheduler_lock);

    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    if(cpu == NULL){
        ticket_unlock(&scheduler_lock);
        return -1;
    }
    new_task->queue_index = queue_enqueue(cpu->scheduler_queue,new_task);
    if(new_task->queue_index == -1){
        logkf("Error: scheduler null %d\n",get_current_cpuid());
        return -1;
    }

    if(new_task->pid == kernel_head_task->pid) goto ret;

    pivfs_update(kernel_head_task);
    ret:
    ticket_unlock(&scheduler_lock);
    return new_task->queue_index;
}

void remove_task(pcb_t task){
    if(task == NULL) return;
    ticket_lock(&scheduler_lock);

    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    if(cpu == NULL){
        ticket_unlock(&scheduler_lock);
        return;
    }
    queue_remove_at(cpu->scheduler_queue,task->queue_index);
    pivfs_update(kernel_head_task);
    ticket_unlock(&scheduler_lock);
}

int get_all_task() {
    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    return cpu != NULL ? cpu->scheduler_queue->size : 0;
}

void change_proccess(registers_t *reg,pcb_t taget){
    switch_page_directory(taget->directory);
    set_kernel_stack(taget->kernel_stack);

    current_task->context0.r15 = reg->r15;
    current_task->context0.r14 = reg->r14;
    current_task->context0.r13 = reg->r13;
    current_task->context0.r12 = reg->r12;
    current_task->context0.r11 = reg->r11;
    current_task->context0.r10 = reg->r10;
    current_task->context0.r9 = reg->r9;
    current_task->context0.r8 = reg->r8;
    current_task->context0.rax = reg->rax;
    current_task->context0.rbx = reg->rbx;
    current_task->context0.rcx = reg->rcx;
    current_task->context0.rdx = reg->rdx;
    current_task->context0.rdi = reg->rdi;
    current_task->context0.rsi = reg->rsi;
    current_task->context0.rbp = reg->rbp;
    current_task->context0.rflags = reg->rflags;
    current_task->context0.rip = reg->rip;
    current_task->context0.rsp = reg->rsp;

    reg->r15 = taget->context0.r15;
    reg->r14 = taget->context0.r14;
    reg->r13 = taget->context0.r13;
    reg->r12 = taget->context0.r12;
    reg->r11 = taget->context0.r11;
    reg->r10 = taget->context0.r10;
    reg->r9 = taget->context0.r9;
    reg->r8 = taget->context0.r8;
    reg->rax = taget->context0.rax;
    reg->rbx = taget->context0.rbx;
    reg->rcx = taget->context0.rcx;
    reg->rdx = taget->context0.rdx;
    reg->rdi = taget->context0.rdi;
    reg->rsi = taget->context0.rsi;
    reg->rbp = taget->context0.rbp;
    reg->rflags = taget->context0.rflags;
    reg->rip = taget->context0.rip;
    reg->rsp = taget->context0.rsp;

    current_task = taget;
}

/**
 * CP_Kernel 默认单核调度器 - 循环公平调度
 * @param reg 当前进程上下文
 */
void scheduler(registers_t *reg){
    if(is_scheduler){
        if(current_task != NULL){
            smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
            if (cpu->iter_node == NULL) {
                iter_head:
                cpu->iter_node = cpu->scheduler_queue->head;
            } else {
                cpu->iter_node = cpu->iter_node->next;
                if(cpu->iter_node == NULL) goto iter_head;
            }
            void *data = NULL;
            if (cpu->iter_node != NULL) {
                data = cpu->iter_node->data;
            }

            pcb_t next = (pcb_t)data;
           // logkf("p: %p\n",next);

            current_task->cpu_clock++;
            if(current_task->time_buf != NULL){
                current_task->cpu_timer += get_time(current_task->time_buf);
                current_task->time_buf = NULL;
            }
            current_task->time_buf = alloc_timer();

            if(current_task->pid != next->pid) {
                disable_scheduler();
                change_proccess(reg,next);
                enable_scheduler();
            }
        }
    }
    send_eoi();
}
