#pragma once

#define TASK_KERNEL_LEVEL         0 // 内核任务 (崩溃后会挂起内核)
#define TASK_SYSTEM_SERVICE_LEVEL 1 // 系统服务 (崩溃后会在终端提示)
#define TASK_APPLICATION_LEVEL    2 // 应用程序

#include "ctype.h"
#include "isr.h"
#include "page.h"
#include "scheduler.h"
#include "timer.h"
#include "tty.h"
#include "fpu.h"
#include "lock_queue.h"

typedef struct thread_control_block *tcb_t;
typedef struct process_control_block *pcb_t;

struct process_control_block {
    char              name[50];     // 进程名
    int               pid_index;    // 线程PID累加索引
    int               pgb_id;       // 进程ID
    lock_queue       *pcb_queue;    // 线程队列
    size_t            queue_index;  // 进程队列索引
    page_directory_t *page_dir;     // 进程页表
    tty_t            *tty;          // tty设备
};

struct thread_control_block {
    pcb_t             parent_group; // 父进程
    uint8_t           task_level;   // 线程权限等级
    int               pid;          // 线程私有 PID
    char              name[50];     // 线程名
    uint64_t          cpu_clock;    // CPU 调度时间片
    uint64_t          cpu_timer;    // CPU 占用时间
    uint64_t          cpu_id;       // 由哪个CPU负责该线程运行
    timer_t          *time_buf;     // 计时器句柄
    TaskContext       context0;     // 线程上下文
    fpu_context_t     fpu_context;  // 浮点寄存器上下文
    bool              fpu_flags;    // 浮点启用标志
    uint64_t          kernel_stack; // 内核栈
    uint64_t          user_stack;   // 用户栈
    uint64_t          mem_usage;    // 内存利用率
    size_t            queue_index;  // 调度队列索引
    size_t            group_index;  // 进程队列索引
};

/**
 * 向当下cpu核心的调度队列增加一个任务
 * @param new_task
 * @return == -1 ? 添加失败 : 返回的队列索引
 */
int  add_task(tcb_t new_task);
void remove_task(tcb_t task);

void  switch_to_user_mode(uint64_t func);

/**
 * 创建内核线程
 * @param _start 线程入口点
 * @param args 传入参数
 * @param name 线程名
 * @param pgb_group == NULL ? 无父进程自动进入System进程 : 添加至指定的进程
 * @return 该线程私有 pid
 */
int   create_kernel_thread(int (*_start)(void *arg), void *args, char *name,pcb_t pgb_group);

/**
 * 创建一个进程
 * @param name 进程名
 * @return 进程指针
 */
pcb_t create_process_group(char* name);

/**
 * 获取当前运行的线程(多核下会获取当前CPU正在调度的线程)
 * @return == NULL ? 线程未初始化 : 线程指针
 */
tcb_t get_current_task();
void  kill_all_proc();
void  kill_proc(pcb_t task);
void  kill_thread(tcb_t tcb);

pcb_t found_pcb(int pid);
/**
 * 从指定进程内查找线程
 * @param pcb 进程
 * @param tid 该进程内的线程id
 * @return == NULL ? 未找到线程 : 线程指针
 */
tcb_t found_thread(pcb_t pcb,int tid);
void  init_pcb();
