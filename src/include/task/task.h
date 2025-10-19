#pragma once

#include "cow_arraylist.h"
#include "fs/vfs.h"
#include "ptrace.h"
#include "types.h"
#include "mem/vma.h"
#include "mem/page.h"

typedef struct process_control_block *pcb_t;
typedef struct thread_control_block  *tcb_t;

struct process_control_block {
    pid_t          pid;           // 进程ID
    char          *name;          // 进程名
    pcb_t          parent;        // 父进程
    size_t         pl_index;      // 进程列表索引
    cow_arraylist *child_threads; // 子线程

    page_directory_t *directory;  // 进程页表
    vma_manager_t  vma_manager;   // VMA 内存管理器

    vfs_node_t     cwd;           // 进程工作目录
};

struct thread_control_block {
    pid_t    tid;          // 线程ID
    pcb_t    process;      // 所属进程
    uint64_t prio;         // 任务优先级
    void    *sched_handle; // 调度器句柄
    size_t   ct_index;     // 子线程列表索引
};

pid_t alloc_pid();
pid_t alloc_tid();
void arch_task_scheduler(struct pt_regs *regs);
void setup_task();
