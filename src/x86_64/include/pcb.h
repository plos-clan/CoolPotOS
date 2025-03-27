#pragma once

#define TASK_KERNEL_LEVEL         0
#define TASK_SYSTEM_SERVICE_LEVEL 1
#define TASK_APPLICATION_LEVEL    2

#include "ctype.h"
#include "isr.h"
#include "page.h"
#include "scheduler.h"
#include "timer.h"
#include "tty.h"
#include "fpu.h"
#include "lock_queue.h"

typedef struct process_control_block *pcb_t;
typedef struct pcb_group_block *pgb_t;

struct pcb_group_block {
    char              name[50];     // 进程组名
    int               pid_index;    // 进程PID累加索引
    int               pgb_id;       // 进程组ID
    lock_queue       *pcb_queue;    // 进程队列
    size_t            queue_index;  // 进程组队列索引
    page_directory_t *page_dir;     // 进程组页表
};

struct process_control_block {
    pgb_t             parent_group; // 父进程组
    uint8_t           task_level;   // 进程优先级
    int               pid;          // 进程私有 PID
    char              name[50];     // 进程名
    uint64_t          cpu_clock;    // CPU 调度时间片
    uint64_t          cpu_timer;    // CPU 占用时间
    uint64_t          cpu_id;       // 由哪个CPU负责该进程运行
    timer_t          *time_buf;     // 计时器句柄
    TaskContext       context0;     // 进程上下文
    fpu_context_t     fpu_context;  // 浮点寄存器上下文
    bool              fpu_flags;    // 浮点启用标志
    uint64_t          kernel_stack; // 内核栈
    uint64_t          user_stack;   // 用户栈
    uint64_t          mem_usage;    // 内存利用率
    tty_t            *tty;          // tty设备
    size_t            queue_index;  // 调度队列索引
    size_t            group_index;  // 进程组队列索引
};

/**
 * 向当下cpu核心的调度队列增加一个任务
 * @param new_task
 * @return == -1 ? 添加失败 : 返回的队列索引
 */
int  add_task(pcb_t new_task);
void remove_task(pcb_t task);

void  switch_to_user_mode(uint64_t func);

/**
 * 创建内核线程
 * @param _start 线程入口点
 * @param args 传入参数
 * @param name 线程名
 * @param pgb_group == NULL ? 无父进程组自动进入System进程组 : 添加至指定的进程组
 * @return 该线程私有 pid
 */
int   create_kernel_thread(int (*_start)(void *arg), void *args, char *name,pgb_t pgb_group);

/**
 * 创建一个进程组
 * @param name 进程组名
 * @return 进程组指针
 */
pgb_t create_process_group(char* name);

/**
 * 获取当前运行的进程(多核下会获取当前CPU正在调度的进程)
 * @return == NULL ? 进程未初始化 : 进程指针
 */
pcb_t get_current_task();
void  kill_all_proc();
void  kill_proc(pcb_t task);
pcb_t found_pcb(int pid);
void  init_pcb();
