#ifndef CPOS_TASK_H
#define CPOS_TASK_H

#include <stdint.h>

#define EFLAGES_DEFAULT ( 1 << 1 )
#define EFLAGES_IF (1 << 9)

typedef struct TSS //任务状态段定义
{
    uint32_t pre_link; //任务链接
    uint32_t esp0, ss0, esp1, ss1, esp2, ssp2; //栈寄存器
    uint32_t cr3; //页目录基地址
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt; //局部描述符表
    uint32_t iomap; //I/O位图
}tss_t;

typedef struct task
{
    tss_t tss;
    uint32_t pid;
}task_t;

task_t *create_task(uint32_t entry,uint32_t esp);

void init_task();

#endif