#ifndef CRASHPOWEROS_TASK_H
#define CRASHPOWEROS_TASK_H

#include "memory.h"

typedef
enum task_state {
    TASK_UNINIT = 0,    // 未初始化
    TASK_SLEEPING = 1,  // 睡眠中
    TASK_RUNNABLE = 2,  // 可运行(也许正在运行)
    TASK_ZOMBIE = 3,    // 僵尸状态
    TASK_DEATH = 4     // 终止状态
} task_state;

struct context {
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eflags;
};

// 进程控制块 PCB
struct task_struct {
    volatile task_state state;  // 进程当前状态
    int pid;           // 进程标识符
    char *name;        // 进程名
    void *stack;         // 进程的内核栈地址
    page_directory_t *pgd_dir;     // 进程页表
    struct context context;     // 进程切换需要的上下文信息
    struct task_struct *next;   // 链表指针
};

int32_t kernel_thread(int (*fn)(void *), void *arg, char *name);

void kthread_exit();

void print_proc();

void init_sched();

void schedule();

void change_task_to(struct task_struct *next);

void task_kill(int pid);

#endif //CRASHPOWEROS_TASK_H
