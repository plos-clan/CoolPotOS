#include "scheduler.h"
#include "page.h"
#include "krlibc.h"
#include "description_table.h"
#include "klog.h"
#include "io.h"

extern void switch_to(struct context *prev, struct context *next); //asmfunc.asm

pcb_t *current_pcb = NULL;
pcb_t *running_proc_head = NULL;
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

void scheduler_process(registers_t *reg){
    io_cli();
    if(current_pcb && can_sche){
        current_pcb->cpu_clock++;
        default_scheduler(reg,current_pcb->next);
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