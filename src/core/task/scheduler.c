#include "scheduler.h"
#include "krlibc.h"
#include "pcb.h"
#include "description_table.h"

pcb_t current_task = NULL;
pcb_t kernel_head_task = NULL;

pcb_t get_current_task(){
    return current_task;
}

void add_task(pcb_t new_task) { //添加进程至调度链
    if (new_task == NULL) return;//参数检查
    pcb_t tailt = kernel_head_task;
    while (tailt->next != kernel_head_task) {//查找调度链尾部节点
        if (tailt->next == NULL) break;
        tailt = tailt->next;
    }
    tailt->next = new_task;//添加新进程
}

void change_proccess(interrupt_frame_t *frame,pcb_t taget){
    switch_page_directory(taget->directory);
    set_kernel_stack(taget->kernel_stack);
    current_task = taget;
}

void scheduler(interrupt_frame_t *frame){
    if(current_task != NULL){
        current_task->cpu_clock++;
        change_proccess(frame,current_task->next);
    }
}
