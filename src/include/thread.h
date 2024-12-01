#pragma once

#include "pcb.h"

struct thread_context{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t eip;
};

struct cp_thread{
    pcb_t *father_pcb; //父进程
    int tid;           //线程ID
    struct thread_context context; //线程上下文
};

typedef struct cp_thread* cp_thread_t;