#include "../include/task.h"
#include "../include/memory.h"
#include "../include/config.h"
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/description_table.h"

task_t *kernel_task;
uint32_t pid_index;

static int tss_init_ring0(task_t *task,uint32_t entry,uint32_t esp)
{

    
    memset(&task->tss,0,sizeof(tss_t));
    task->tss.eip = entry;
    task->tss.esp = task->tss.esp0 = esp;
    task->tss.ss = task->tss.ss0 = KERNEL_SECTION_DS;
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = KERNEL_SECTION_DS;
    task->tss.cs = KERNEL_SECTION_CS;
    task->tss.eflags = EFLAGES_IF | EFLAGES_DEFAULT;
    return 0;
}

task_t *create_task_ring0(uint32_t entry,uint32_t esp)
{
    task_t *task = (task_t*)kmalloc(sizeof(task_t));
    tss_init_ring0(task,entry,esp);
    task->pid = pid_index++;
    return task;
}

void debug(){
    for(;;)
        printf("Hello!");
}

void init_task(){
    io_cli();
    kernel_task = create_task_ring0(0,0);
    task_t* debug_task = create_task_ring0(debug,kmalloc(1024));
    //write_tss(kernel_task->tss);
    io_sti();
}