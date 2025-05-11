#pragma once

#define TASK_KERNEL_LEVEL         0 // 内核任务 (崩溃后会挂起内核)
#define TASK_SYSTEM_SERVICE_LEVEL 1 // 系统服务 (崩溃后会在终端提示)
#define TASK_APPLICATION_LEVEL    2 // 应用程序

#include "ctype.h"
#include "fpu.h"
#include "isr.h"
#include "lock_queue.h"
#include "page.h"
#include "scheduler.h"
#include "syscall.h"
#include "sysuser.h"
#include "timer.h"
#include "tty.h"

typedef struct thread_control_block  *tcb_t;
typedef struct process_control_block *pcb_t;

typedef enum {
    CREATE  = 0, // 创建中
    RUNNING = 1, // 运行中
    WAIT    = 2, // 线程阻塞
    DEATH   = 3, // 死亡(无法被调度, 线程状态为等待处死)
    START   = 4, // 准备调度
    OUT     = 5, // 已被处死(线程状态)
} TaskStatus;

struct process_control_block {
    char              name[50];    // 进程名
    uint8_t           task_level;  // 进程权限等级
    int               pid_index;   // 线程PID累加索引
    int               pgb_id;      // 进程ID
    lock_queue       *pcb_queue;   // 线程队列
    size_t            queue_index; // 进程队列索引
    size_t            death_index; // 死亡队列索引
    lock_queue       *ipc_queue;   // 进程消息队列
    lock_queue       *file_open;   // 文件句柄占用队列
    page_directory_t *page_dir;    // 进程页表
    ucb_t             user;        // 用户会话
    tty_t            *tty;         // TTY设备
    TaskStatus        status;      // 进程状态
    uint64_t          mmap_start;  // 映射起始地址
    pcb_t             parent_task; // 父进程
};

struct thread_control_block {
    pcb_t      parent_group; // 父进程
    uint8_t    task_level;   // 线程权限等级
    int        pid;          // 线程 TID
    TaskStatus status;       // 线程状态
    char       name[50];     // 线程名

    uint64_t      cpu_clock;    // CPU 调度时间片
    uint64_t      cpu_timer;    // CPU 占用时间
    uint64_t      cpu_id;       // 由哪个CPU负责该线程运行
    timer_t      *time_buf;     // 计时器句柄
    TaskContext   context0;     // 线程上下文
    fpu_context_t fpu_context;  // 浮点寄存器上下文
    bool          fpu_flags;    // 浮点启用标志
    uint64_t      main;         // 入口函数地址
    uint64_t      kernel_stack; // 内核栈
    uint64_t      user_stack;   // 用户栈
    uint64_t      mem_usage;    // 内存利用率

    size_t queue_index; // 调度队列索引
    size_t group_index; // 进程队列索引
    size_t death_index; // 死亡队列索引

    int seq_state; // 键盘状态标志
    int last_key;  // 标志按键
};

static __attr(address_space(257)) struct thread_control_block *const tcb =
    (__attr(address_space(257)) void *)0;

/**
 * 向当下cpu核心的调度队列增加一个任务
 * @param new_task
 * @return == -1 ? 添加失败 : 返回的队列索引
 */
int  add_task(tcb_t new_task);
void remove_task(tcb_t task);

void switch_to_user_mode(uint64_t func);

/**
 * 创建内核线程
 * @param _start 线程入口点
 * @param args 传入参数
 * @param name 线程名
 * @param pgb_group == NULL ? 无父进程自动进入System进程 : 添加至指定的进程
 * @return 线程 tid
 */
int create_kernel_thread(int (*_start)(void *arg), void *args, char *name, pcb_t pgb_group);

/**
 * 创建用户态线程
 * @param _start 线程入口点
 * @param name 线程名
 * @param pcb 父进程(父进程页表不能使用内核页表, 需调用clone_directory拷贝一个新页表使用)
 * @return == -1 ? 创建失败 : 线程 tid
 */
int create_user_thread(void (*_start)(void), char *name, pcb_t pcb);

/**
 * 创建一个进程
 * @param name 进程名
 * @param directory == NULL ? 使用内核页表 : 使用新页表
 * @param user_handle == NULL ? 内核会话 ? 指定用户会话
 * @return 进程指针
 */
pcb_t create_process_group(char *name, page_directory_t *directory, ucb_t user_handle);

/**
 * 获取当前运行的线程(多核下会获取当前CPU正在调度的线程)
 * @return == NULL ? 线程未初始化 : 线程指针
 */
tcb_t get_current_task();
void  kill_all_proc();

/**
 * 结束指定线程
 * @param task 进程控制块
 * @param exit_code 退出代码
 */
void kill_proc(pcb_t task, int exit_code);
void kill_thread(tcb_t tcb);

/**
 * 以指定PID查找进程
 * @param pid 进程ID
 * @return == NULL ? 未找到进程 : 进程指针
 */
pcb_t found_pcb(int pid);

/**
 * 从指定进程内查找线程
 * @param pcb 进程
 * @param tid 该进程内的线程id
 * @return == NULL ? 未找到线程 : 线程指针
 */
tcb_t found_thread(pcb_t pcb, int tid);

/**
 * 等待指定PID进程执行完毕
 * @param pid 线程ID
 * @return == -25565 ? 未找到线程 : 退出代码
 */
int waitpid(int pid);

void init_pcb();

void kill_proc0(pcb_t pcb);
void kill_thread0(tcb_t task);

void change_current_tcb(tcb_t new_tcb);
