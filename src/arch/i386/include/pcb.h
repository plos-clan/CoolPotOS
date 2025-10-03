#pragma once

#define TASK_KERNEL_LEVEL         0
#define TASK_SYSTEM_SERVICE_LEVEL 1
#define TASK_APPLICATION_LEVEL    2

#include "ctype.h"
#include "elf.h"
#include "page.h"
#include "syscall.h"
#include "tty.h"
#include "vfs.h"

enum PStatus {
    RUNNING,
    DEATH,
    WAIT,
};

typedef struct cp_thread *tcb_t;

typedef struct __attribute__((packed)) fpu_regs {
    uint16_t control;
    uint16_t RESERVED1;
    uint16_t status;
    uint16_t RESERVED2;
    uint16_t tag;
    uint16_t RESERVED3;
    uint32_t fip0;
    uint32_t fop0;
    uint32_t fdp0;
    uint32_t fdp1;
    uint8_t  regs[80];
} fpu_regs_t;

struct context {
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ecx;
    uint32_t edx;
    uint32_t eflags;

    uint32_t   eax;
    uint32_t   eip;
    uint32_t   ds;
    uint32_t   cs;
    uint32_t   ss;
    fpu_regs_t fpu_regs;
};

typedef struct task_pcb {
    uint8_t           task_level; // 进程等级< 0:内核 | 1:系统服务 | 2:应用程序 >
    int               pid;
    char              name[50];
    char              cmdline[50];       // 命令行实参
    char             *user_cmdline;      // 用户空间的命令行实参
    void             *user_stack;        // 用户栈
    void             *kernel_stack;      // 内核栈
    void             *program_break;     // 进程堆基址
    void             *program_break_end; // 进程堆尾
    page_directory_t *pgd_dir;           // 进程页表
    struct context    context;           // 上下文信息
    tty_t            *tty;               // TTY设备
    bool              fpu_flag;          // 是否使用 FPU
    int               now_tid;
    enum PStatus      status;           // 进程状态
    uint32_t          cpu_clock;        // CPU运行时间片
    uint32_t          sche_time;        // 进程剩余的可运行时间片
    cfile_t           file_table[255];  // 进程文件句柄表
    vfs_node_t        exe_file;         // 可执行文件
    Elf32_Ehdr       *data;             // 可执行文件elf句柄
    tcb_t             main_thread_head; // 线程队列
    tcb_t             current_thread;   // 当前调度线程
    struct task_pcb  *next;             // 链表指针
} pcb_t;

void process_exit();

void switch_to_user_mode(uint32_t func);

void   kill_all_proc(); // 终止所有进程
int    create_user_process(const char *path, const char *cmdline, char *name,
                           uint8_t level);                                     // 创建用户态进程
int    create_kernel_thread(int (*_start)(void *arg), void *args, char *name); // 创建内核线程
void   kill_proc(pcb_t *pcb);                                                  // 杀死指定进程
pcb_t *found_pcb(int pid); // 根据PID获取一个进程
void   init_pcb();