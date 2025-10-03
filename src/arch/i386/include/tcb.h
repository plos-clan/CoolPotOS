#pragma once

#include "pcb.h"

struct thread_context {
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ecx;
    uint32_t edx;
    uint32_t eflags;
};

struct cp_thread {
    pcb_t                *father_pcb;   // 父进程
    int                   tid;          // 线程ID
    void                 *user_stack;   // 用户栈
    void                 *kernel_stack; // 内核栈
    struct thread_context context;      // 线程上下文
    tcb_t                 next;         // 线程链表
};

tcb_t create_user_thread(pcb_t *pcb, void *func);

tcb_t create_kernel_subthread(pcb_t *pcb, void *func);

void kill_user_thread(pcb_t *pcb, tcb_t thread);