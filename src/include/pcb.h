#pragma once

#define TASK_KERNEL_LEVEL 0
#define TASK_SYSTEM_SERVICE_LEVEL 1
#define TASK_APPLICATION_LEVEL 2

#include "ctype.h"
#include "isr.h"
#include "page.h"
#include "tty.h"
#include "scheduler.h"
#include "timer.h"

struct process_control_block{
    uint8_t task_level;           // 进程优先级
    int pid;                      // 进程 PID
    char name[50];                // 进程名
    uint64_t cpu_clock;           // CPU 调度时间片
    uint64_t cpu_timer;           // CPU 占用时间
    uint64_t cpu_id;              // 由哪个CPU负责该进程运行
    timer_t *time_buf;            // 计时器句柄
    TaskContext context0;         // 进程上下文
    page_directory_t *directory;  // 进程页目录
    uint64_t kernel_stack;        // 内核栈
    uint64_t user_stack;          // 用户栈
    uint64_t mem_usage;           // 内存利用率
    tty_t *tty;                   // tty设备
    struct process_control_block *next; // 下一个进程
};

typedef struct process_control_block *pcb_t;

void add_task(pcb_t new_task);
void switch_to_user_mode(uint64_t func);
int create_kernel_thread(int (*_start)(void *arg), void *args, char *name);
pcb_t get_current_task();
void kill_all_proc();
void kill_proc(pcb_t task);
pcb_t found_pcb(int pid);
void init_pcb();
