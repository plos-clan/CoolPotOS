#include "scheduler.h"
#include "page.h"
#include "krlibc.h"
#include "description_table.h"
#include "klog.h"
#include "io.h"
#include "timer.h"

extern void switch_to(struct context *prev, struct context *next); //asmfunc.asm

pcb_t *current_pcb = NULL; //当前运行进程
pcb_t *running_proc_head = NULL; //调度队列
pcb_t *wait_proc_head = NULL; //等待队列

bool can_sche = false; //调度标志位

int get_all_task(){
    int num = 1;
    pcb_t *pcb = running_proc_head;
    do{
        pcb = pcb->next;
        if(pcb == NULL) break;
        num++;
    } while (1);
    return num;
}

void enable_scheduler(){
    can_sche = true;
}

void disable_scheduler(){
    can_sche = false;
}

pcb_t *get_current_proc(){
    return current_pcb;
}

void kernel_sche(){
    __asm__("int $31\n");
}

void scheduler_process(registers_t *reg){
    io_cli();
    if(current_pcb && can_sche){
        current_pcb->cpu_clock++;
        if(current_pcb->task_level == TASK_KERNEL_LEVEL){
            default_scheduler(reg,current_pcb->next);
        } else{
            //TODO 用户线程调度
            default_scheduler(reg,current_pcb->next);
        }

    }
}

void default_scheduler(registers_t *reg,pcb_t* next){ //CP_Kernel 默认的进程调度器

    if(current_pcb->sche_time > 1){
        current_pcb->sche_time--;
        return;
    }

    if (current_pcb != next) {
        if(next == NULL) next = running_proc_head;
        current_pcb->sche_time = 1;
        pcb_t *prev = current_pcb;
        current_pcb = next;
        switch_page_directory(current_pcb->pgd_dir);
        set_kernel_stack((uintptr_t)current_pcb->kernel_stack + STACK_SIZE);
        prev->context.eip = reg->eip;
        prev->context.ds = reg->ds;
        prev->context.cs = reg->cs;
        prev->context.eax = reg->eax;
        prev->context.ss = reg->ss;

        switch_to(&(prev->context), &(current_pcb->context));

        reg->ds = current_pcb->context.ds;
        reg->cs = current_pcb->context.cs;
        reg->eip = current_pcb->context.eip;
        reg->eax = current_pcb->context.eax;
        reg->ss = current_pcb->context.ss;
    }
}