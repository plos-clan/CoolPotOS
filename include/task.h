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
    struct context context;     // 上下文信息
    struct task_struct *next;   // 链表指针
};

void print_proc_t(int *i,struct task_struct *base,struct task_struct *cur,int is_print);

struct task_struct* get_current();

int32_t kernel_thread(int (*fn)(void *), void *arg, char *name);

void kthread_exit();

void print_proc();

void init_sched();

void schedule();

void change_task_to(struct task_struct *next);

void task_kill(int pid);

void kill_all_task();

struct task_struct* found_task_pid(int pid);

void wait_task(struct task_struct *task);

void start_task(struct  task_struct *task);

#endif //CRASHPOWEROS_TASK_H
