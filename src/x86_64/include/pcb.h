#pragma once

#define TASK_KERNEL_LEVEL      0 // 内核任务 (崩溃后会挂起内核)
#define TASK_IDLE_LEVEL        1 // IDLE (崩溃后与普通内核任务相同行为)
#define TASK_APPLICATION_LEVEL 2 // 应用程序

#define PR_SET_PDEATHSIG            1
#define PR_GET_PDEATHSIG            2
#define PR_GET_DUMPABLE             3
#define PR_SET_DUMPABLE             4
#define PR_GET_UNALIGN              5
#define PR_SET_UNALIGN              6
#define PR_GET_KEEPCAPS             7
#define PR_SET_KEEPCAPS             8
#define PR_GET_FPEMU                9
#define PR_SET_FPEMU                10
#define PR_GET_FPEXC                11
#define PR_SET_FPEXC                12
#define PR_GET_TIMING               13
#define PR_SET_TIMING               14
#define PR_SET_NAME                 15
#define PR_GET_NAME                 16
#define PR_GET_ENDIAN               19
#define PR_SET_ENDIAN               20
#define PR_GET_SECCOMP              21
#define PR_SET_SECCOMP              22
#define PR_CAPBSET_READ             23
#define PR_CAPBSET_DROP             24
#define PR_SET_NO_NEW_PRIVS         38
#define PR_GET_NO_NEW_PRIVS         39
#define PR_MCE_KILL                 33
#define PR_MCE_KILL_GET             34
#define PR_SET_MM                   35
#define PR_SET_PTRACER              0x59616d61 // 'Yama' magic value
#define PR_SET_THP_DISABLE          41
#define PR_GET_THP_DISABLE          42
#define PR_TASK_PERF_EVENTS_DISABLE 31
#define PR_TASK_PERF_EVENTS_ENABLE  32
#define PR_GET_SPECULATION_CTRL     52
#define PR_SET_SPECULATION_CTRL     53

#define CLONE_VM             0x00000100 /* set if VM shared between processes */
#define CLONE_FS             0x00000200 /* set if fs info shared between processes */
#define CLONE_FILES          0x00000400 /* set if open files shared between processes */
#define CLONE_SIGHAND        0x00000800 /* set if signal handlers and blocked signals shared */
#define CLONE_PIDFD          0x00001000 /* set if a pidfd should be placed in parent */
#define CLONE_PTRACE         0x00002000 /* set if we want to let tracing continue on the child too */
#define CLONE_VFORK          0x00004000 /* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT         0x00008000 /* set if we want to have the same parent as the cloner */
#define CLONE_THREAD         0x00010000 /* Same thread group? */
#define CLONE_NEWNS          0x00020000 /* New mount namespace group */
#define CLONE_SYSVSEM        0x00040000 /* share system V SEM_UNDO semantics */
#define CLONE_SETTLS         0x00080000 /* create a new TLS for the child */
#define CLONE_PARENT_SETTID  0x00100000 /* set the TID in the parent */
#define CLONE_CHILD_CLEARTID 0x00200000 /* clear the TID in the child */
#define CLONE_DETACHED       0x00400000 /* Unused, ignored */
#define CLONE_UNTRACED                                                                             \
    0x00800000 /* set if the tracing process can't force CLONE_PTRACE on this clone */
#define CLONE_CHILD_SETTID 0x01000000 /* set the TID in the child */
#define CLONE_NEWCGROUP    0x02000000 /* New cgroup namespace */
#define CLONE_NEWUTS       0x04000000 /* New utsname namespace */
#define CLONE_NEWIPC       0x08000000 /* New ipc namespace */
#define CLONE_NEWUSER      0x10000000 /* New user namespace */
#define CLONE_NEWPID       0x20000000 /* New pid namespace */
#define CLONE_NEWNET       0x40000000 /* New network namespace */
#define CLONE_IO           0x80000000 /* Clone io context */

#include "ctype.h"
#include "fpu.h"
#include "isr.h"
#include "lock_queue.h"
#include "page.h"
#include "scheduler.h"
#include "signal.h"
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
    FUTEX   = 5, // 被挂起(无法被调度, 线程状态为等待唤醒)
    OUT     = 6, // 已被处死(无法被调度)
} TaskStatus;

typedef struct {
    void  *sp;
    size_t size;
    int    flags;
} altstack_t; // 信号备用栈

struct process_control_block {
    char              name[50];    // 进程名
    uint8_t           task_level;  // 进程权限等级
    int               pid;         // 进程ID
    char             *cmdline;     // 命令行参数
    char             *cwd;         // 工作目录路径
    lock_queue       *pcb_queue;   // 线程队列
    size_t            queue_index; // 进程队列索引
    size_t            death_index; // 死亡队列索引
    lock_queue       *ipc_queue;   // 进程消息队列
    lock_queue       *file_open;   // 文件句柄占用队列
    lock_queue       *virt_queue;  // 虚拟页分配队列
    page_directory_t *page_dir;    // 进程页表
    ucb_t             user;        // 用户会话
    tty_t            *tty;         // TTY设备
    TaskStatus        status;      // 进程状态
    uint64_t          mmap_start;  // 映射起始地址
    pcb_t             parent_task; // 父进程
    void             *elf_file;    // 可执行文件指针
    size_t            elf_size;    // 可执行文件大小
    uint64_t          load_start;  // 加载起始地址
    bool              vfork;       // 是否是 vfork 创建的进程
};

struct thread_control_block {
    pcb_t      parent_group; // 父进程
    uint8_t    task_level;   // 线程权限等级
    int        tid;          // 线程 TID
    TaskStatus status;       // 线程状态
    char       name[50];     // 线程名

    sigaction_t   actions[MAXSIG]; // 信号处理器回调
    uint64_t      signal;          // 信号位图
    uint64_t      blocked;         // 屏蔽位图
    uint64_t      cpu_clock;       // CPU 调度时间片
    uint64_t      cpu_timer;       // CPU 占用时间
    uint64_t      cpu_id;          // 由哪个CPU负责该线程运行
    timer_t      *time_buf;        // 计时器句柄
    TaskContext   context0;        // 线程上下文
    fpu_context_t fpu_context;     // 浮点寄存器上下文
    bool          fpu_flags;       // 浮点启用标志
    uint64_t      main;            // 入口函数地址
    uint64_t      kernel_stack;    // 内核栈
    uint64_t      user_stack;      // 用户栈
    uint64_t      user_stack_top;  // 用户栈顶部地址
    uint64_t      mem_usage;       // 内存利用率
    uint64_t      affinity_mask;   // 线程亲和性掩码
    altstack_t    alt_stack;       // 信号备用栈

    uint64_t          tid_address;   // 线程ID地址
    page_directory_t *tid_directory; // 线程id所属页表
    uint64_t          fs_base;       // fs段基址
    uint64_t          gs_base;       // gs段基址
    uint64_t          fs, gs;

    size_t queue_index; // 调度队列索引
    size_t group_index; // 进程队列索引
    size_t death_index; // 死亡队列索引
    size_t futex_index; // 挂起队列索引

    void *sched_handle; // 调度器句柄
};

static __attr(address_space(257)) struct thread_control_block *const tcb =
    (__attr(address_space(257)) void *)0;

/**
 * 由分配算法决定向哪个核心添加任务
 * @param new_task
 * @return == -1 ? 添加失败 : 返回的队列索引
 */
int  add_task(tcb_t new_task);
void remove_task(tcb_t task);

/**
 * 向指定CPU核心添加一个任务
 */
int add_task_cpu(tcb_t new_task, size_t cpuid);

void switch_to_user_mode(uint64_t func);

/**
 * 创建内核线程
 * @param _start 线程入口点
 * @param args 传入参数
 * @param name 线程名
 * @param pgb_group == NULL ? 无父进程自动进入System进程 : 添加至指定的进程
 * @param cpuid 向指定CPU核心添加线程, (SIZE_MAX为自动分配)
 * @return 线程 tid
 */
int create_kernel_thread(int (*_start)(void *arg), void *args, char *name, pcb_t pgb_group,
                         size_t cpuid);

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
 * @param cmdline 命令行参数 (不得为NULL,无参数放空字符串)
 * @param parent_process 父进程(为NULL自动寻父)
 * @param elf_file 可执行文件指针 (用户态程序不得为NULL)
 * @param elf_size 可执行文件大小 (用户态程序不得为0)
 * @return 进程指针
 */
pcb_t create_process_group(char *name, page_directory_t *directory, ucb_t user_handle,
                           char *cmdline, pcb_t parent_process, void *elf_file, size_t elf_size);

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
 * @param pid_ret 设置实际监听的进程 PID
 * @return == -25565 ? 未找到线程 : 退出代码
 */
int waitpid(int pid, int *pid_ret);

/**
 * syscall_prctl 系统调用实现
 * @param option 控制选项
 * @param arg2
 * @param arg3
 * @param arg4
 * @param arg5
 * @return 成功返回 0 (失败返回 -1)
 */
int process_control(int option, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

/**
 * clone 系统调用实现
 * @param reg 系统调用上下文
 * @param flags 克隆标志
 * @param stack 用户栈
 * @param parent_tid 父线程 id
 * @param child_tid 子线程 id
 * @param tls TLS 地址
 * @return 线程 tid
 */
uint64_t thread_clone(struct syscall_regs *reg, uint64_t flags, uint64_t stack, int *parent_tid,
                      int *child_tid, uint64_t tls);

/**
 * fork vfork 系统调用实现
 * @param is_vfork 是否是 vfork
 * @param user_stack 不为 NULL 时为 clone 带 vfork 标志实现
 * @return 新进程的 PID
 */
uint64_t process_fork(struct syscall_regs *reg, bool is_vfork, uint64_t user_stack);

/**
 * execve 系统调用实现
 * @param path 可执行程序路径
 * @param argv 可执行程序参数
 * @param envp 环境变量
 * @return 有返回代表失败, 否则当前进程完全更改为新进程
 */
uint64_t process_execve(char *path, char **argv, char **envp);

/**
 * 将一个线程添加到挂起队列
 * @param phys_addr 用户态锁物理地址
 * @param thread 线程
 */
void futex_add(void *phys_addr, tcb_t thread);

/**
 * 唤醒指定锁地址的线程
 * @param phys_addr 用户态锁物理地址
 * @param count 唤醒数量
 */
void futex_wake(void *phys_addr, int count);

/**
 * 释放挂起队列的线程引用
 * @param thread 线程
 */
void futex_free(tcb_t thread);

/**
 * 初始化任务负载权重
 * @param thread 被初始化的任务
 */
void set_load_wight(tcb_t thread);

/**
 * 更新虚拟时间
 * @param thread 被更新线程
 */
void update_vruntime(tcb_t thread);

int task_block(tcb_t thread, TaskStatus state, int timeout_ms);

void init_pcb();
void futex_init();

void kill_proc0(pcb_t pcb);
void kill_thread0(tcb_t task);

void change_current_tcb(tcb_t new_tcb);
