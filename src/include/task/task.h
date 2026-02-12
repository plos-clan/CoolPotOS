#pragma once

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

typedef struct process_control_block *pcb_t;
typedef struct thread_control_block  *tcb_t;

#include "arch_context.h"
#include "cow_arraylist.h"
#include "driver/tty.h"
#include "fs/fds.h"
#include "fs/vfs.h"
#include "ipc.h"
#include "llist_queue.h"
#include "mem/page.h"
#include "mem/vma.h"
#include "metadata.h"
#include "ptrace.h"
#include "signal.h"
#include "timer.h"
#include "types.h"

typedef enum {
    T_CREATE  = 0, // 创建中
    T_RUNNING = 1, // 运行中
    T_WAIT    = 2, // 线程阻塞
    T_DEATH   = 3, // 死亡(无法被调度, 线程状态为等待处死)
    T_START   = 4, // 准备调度
    T_FUTEX   = 5, // 被挂起(无法被调度, 线程状态为等待唤醒)
    T_OUT     = 6, // 已被处死(无法被调度)
    T_ZOMBIE =
        7, // 僵尸进程(无法被调度, 进程已终止, 但其父进程尚未调用 wait/waitpid 获取其退出状态)
} task_status;

struct process_control_block {
    pid_t          pid;           // 进程ID
    pid_t          pgid;          // 进程组ID
    pid_t          sid;           // 会话ID
    char          *name;          // 进程名
    char          *cmdline;       // 命令行完整形参
    size_t         cl_length;     // 命令行形参长度
    pcb_t          parent;        // 父进程
    size_t         pl_index;      // 进程列表索引
    size_t         ppl_index;     // 子进程列表索引
    cow_arraylist *child_threads; // 子线程
    cow_arraylist *child_process; // 子进程
    task_status    status;        // 进程状态

    page_directory_t *directory;   // 进程页表
    vma_manager_t     vma_manager; // VMA 内存管理器
    list_queue_t     *virt_queue;  // 懒分配器队列

    ipc_queue_t *ipc_queue;   // 进程消息队列
    tty_t       *tty;         // 进程占用的TTY会话
    char        *ctty_path;   // 控制终端路径 (如 "/dev/pts/3")
    vfs_node_t   cwd;         // 进程工作目录
    vfs_node_t   exec;        // 可执行文件句柄
    vfs_node_t   procfs_node; // 进程信息虚拟文件系统节点
    vfs_node_t   proc_root;   // 进程根节点
    fdt_t       *fdts;        // 文件描述符表
    char       **envp;        // 进程环境变量
    size_t       envc;        // 进程环境变量长度
    bool         vfork;       // 是否是 vfork 出来的进程

    int_timer_internal_t itimer_real;

    int      uid; // 用户会话ID
    int      euid;
    int      ruid;
    int      egid;
    int      rgid;
    int      sgid;
    uint16_t umask;
};

struct thread_control_block {
    uint64_t             syscall_stack;      // 系统调用栈顶地址
    uint64_t             syscall_stack_user; // 用户态下系统调用栈缓存
    uint64_t             signal_stack;       // 信号栈顶地址
    uint64_t             call_in_signal;     // 是否在信号处理过程
    struct arch_context_ context;            // 任务上下文

    uint64_t          tid_address;   //
    page_directory_t *tid_directory; //
    char             *name;          // 线程名
    pid_t             tid;           // 线程ID
    pcb_t             process;       // 所属进程
    uint64_t          prio;          // 任务优先级
    void             *sched_handle;  // 调度器句柄
    size_t            ct_index;      // 子线程列表索引
    task_status       status;        // 线程状态
    uint64_t          _start;        // 线程入口函数
    uint64_t          affinity_mask; // 线程亲和性掩码

    sigaction_t actions[MAXSIG];   // 信号处理器回调
    uint64_t    signal;            // 信号位图
    uint64_t    blocked;           // 屏蔽位图
    uint64_t    saved_sigmask;     // sigsuspend 保存的原始信号掩码
    bool        has_saved_sigmask; // 是否需要恢复 saved_sigmask
    altstack_t  alt_stack;         // 信号备用栈

    size_t   cpu_id; // 线程所属CPUID
    size_t   futex_index;
    uint64_t sleep_deadline; // nanosleep 唤醒时间 (nano_time)
};

pid_t          alloc_pid();
pid_t          alloc_tid();
tcb_t          get_current_task();
void           arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs);
void           arch_context_init(tcb_t                 thread,
                                 struct arch_context_ *context); // 该函数仅适用于 idle 任务的上下文初始化
void           arch_context_init_thread(tcb_t thread, void *arg); // 用于初始化线程上下文
_Noreturn void arch_switch_to_user_mode();                        // 架构实现切换至用户态
void           arch_context_free(tcb_t thread);                   // 架构实现释放上下文
pid_t          create_process(const char *name, pcb_t parent, uint64_t flags);
pid_t          create_kernel_thread(const char *name,
                                    int (*func)(void *arg),
                                    void    *arg,
                                    pcb_t    process,
                                    uint64_t prio);
int            waitpid(pid_t pid, pid_t *pid_ret, bool nohang);
void           kill_thread(tcb_t task);
void           kill_proc(pcb_t pcb, int exit_code, bool is_zombie);
bool           signals_pending_quick(tcb_t task); // signal.c
pcb_t          found_pcb(pid_t pid);
void           setup_task();
