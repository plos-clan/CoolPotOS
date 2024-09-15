#ifndef CRASHPOWEROS_TASK_H
#define CRASHPOWEROS_TASK_H

#define TASK_KERNEL_LEVEL 0
#define TASK_SYSTEM_SERVICE_LEVEL 1
#define TASK_APPLICATION_LEVEL 2

#include "memory.h"
#include "vfs.h"
#include "tty.h"

typedef struct {
    int (*main)(int argc,char* * argv);
}user_func_t;

typedef
enum task_state {
    TASK_UNINIT = 0,    // 未初始化
    TASK_SLEEPING = 1,  // 睡眠中
    TASK_RUNNABLE = 2,  // 可运行(也许正在运行)
    TASK_ZOMBIE = 3,    // 僵尸状态
    TASK_DEATH = 4     // 终止状态
} task_state;

struct context{
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ecx;
    uint32_t edx;
    uint32_t eflags;

    uint32_t eax;
    uint32_t eip;
    uint32_t ds;
    uint32_t cs;
    uint32_t ss;
};

// 进程控制块 PCB
struct task_struct {
    uint8_t task_level;           // 进程等级< 0:内核 | 1:系统服务 | 2:应用程序 >
    volatile task_state state;    // 进程当前状态
    int pid;                      // 进程标识符
    int mem_size;                 // 内存利用率
    char *name;                   // 进程名
    void *stack;                  // 进程的内核栈地址
    header_t *head;               // 进程堆链表头
    header_t *tail;
    tty_t *tty;                   // 输入输出设备管理器
    vfs_t *vfs_now;               // 文件路径焦点
    bool isUser;                  // 是否是用户进程
    uint32_t program_break;       // 进程堆基址
    uint32_t program_break_end;   // 进程堆尾
    page_directory_t *pgd_dir;    // 进程页表
    struct context context;       // 上下文信息
    struct task_struct *next;     // 链表指针
    char* argv;                   // 命令行参数
    uint32_t cpu_clock;           // CPU运行时间片
    uint32_t page_alloc_address;  // 页分配计数器
};

void print_proc_t(int *i,struct task_struct *base,struct task_struct *cur,int is_print);

struct task_struct* get_current();

int32_t kernel_thread(int (*fn)(void *), void *arg, char *name);

void kthread_exit();

void print_proc();

void init_sched();

void schedule(registers_t *reg);

void change_task_to(registers_t *reg,struct task_struct *next);

void task_kill(int pid);

void kill_all_task();

struct task_struct* found_task_pid(int pid);

void wait_task(struct task_struct *task);

void start_task(struct  task_struct *task);

int get_procs();

void switch_to_user_mode(uint32_t func);

int32_t user_process(char *path, char *name,char* argv,uint8_t level);
#endif //CRASHPOWEROS_TASK_H
