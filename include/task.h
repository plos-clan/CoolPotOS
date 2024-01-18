#ifndef CPOS_TASK_H
#define CPOS_TASK_H

#include <stdint.h>
#include "memory.h"

#define EFLAGES_DEFAULT ( 1 << 1 )
#define EFLAGES_IF (1 << 9)

/*
        通用寄存器(EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
        段寄存器(ES，CS，SS，DS，FS，GS)
        状态寄存器(EFLAGS)
        指令指针(EIP)
        前一个执行的任务的TSS段的选择子(只有当要返回时才更新)
*/

struct tss_entry {
    uint32_t link;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint16_t padding1;
    uint16_t iopb_off;
} __attribute__ ((packed));

typedef struct task
{
    struct tss_entry tss;
    uint32_t pid;
    char* stack; //内核堆栈
    char* task_name;
    page_directory_t *pgd; //进程页目录
}task_t;

void tss_install();
void tss_set(uint16_t ss0, uint32_t esp0);
void init_task();

#endif