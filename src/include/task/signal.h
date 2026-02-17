#pragma once

#define SIGHUP 1   // Hangup (终端断开)
#define SIGINT 2   // Interrupt (Ctrl+C)
#define SIGQUIT 3  // Quit (Ctrl+\)
#define SIGILL 4   // Illegal instruction
#define SIGTRAP 5  // Trace trap
#define SIGABRT 6  // Abort
#define SIGBUS 7   // Bus error
#define SIGFPE 8   // Floating-point exception
#define SIGKILL 9  // Kill signal (不可捕获/屏蔽)
#define SIGUSR1 10 // User-defined signal 1
#define SIGSEGV 11 // Segmentation fault
#define SIGUSR2 12 // User-defined signal 2
#define SIGPIPE 13 // Broken pipe
#define SIGALRM 14 // Alarm clock
#define SIGTERM 15 // Termination signal
#define SIGSTKFLT 16
#define SIGCHLD 17 // Child stopped or terminated
#define SIGCONT 18 // Continue if stopped
#define SIGSTOP 19 // Stop process (不可捕获)
#define SIGTSTP 20 // Stop typed at terminal (Ctrl+Z)
#define SIGTTIN 21 // Background read from tty
#define SIGTTOU 22 // Background write to tty
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGPOLL 29
#define SIGIO 29
#define SIGPWR 30
#define SIGSYS 31
#define SIGUNUSED SIGSYS
#define SIGIOT SIGABRT

#define SIG_NOMASK 0x40000000
#define SIG_ONESHOT 0x80000000

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO 0x00000004
#define SA_RESTORER 0x04000000
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000

#define MINSIG 1
#define MAXSIG 32

#define HAS_SIGNAL(sigset, signum) (sigset & (1ULL << signum))
#define SIGMASK(sig) (1 << (sig))

#define SIG_DFL ((sighandler_t)0) // 默认的信号处理程序（信号句柄）
#define SIG_IGN ((sighandler_t)1) // 忽略信号的处理程序

#define MAX_SIGNALS 64 // 最大支持信号个数

#include "types.h"

typedef struct signal_block signal_block_t;
typedef uint64_t            sigset_t;
typedef void (*sighandler_t)(void);

typedef struct sigaction {
    sighandler_t  sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
} sigaction_t;

typedef struct {
    void  *sp;
    size_t size;
    int    flags;
} altstack_t; // 信号备用栈

struct signal_block {
    void (*signal_handlers[MAX_SIGNALS])(int); // 每个信号对应的用户处理器
    uint64_t    pending_signals;               // 用 bitmap 表示待处理信号
    uint64_t    blocked;                       // 屏蔽的信号
    sigaction_t actions[MAXSIG];
} __attribute__((packed));

typedef enum signal_internal {
    SIGNAL_INTERNAL_CORE = 0,
    SIGNAL_INTERNAL_TERM,
    SIGNAL_INTERNAL_IGN,
    SIGNAL_INTERNAL_STOP,
    SIGNAL_INTERNAL_CONT
} signal_internal_t;

typedef struct {
    int32_t si_signo; // Signal number
    int32_t si_errno; // Error number (if applicable)
    int32_t si_code;  // Signal code

    union {
        int32_t _pad[128 - 3 * sizeof(int32_t) / sizeof(int32_t)];

        // Kill
        struct {
            int32_t  si_pid; // Sending process ID
            uint32_t si_uid; // Real user ID of sending process
        } _kill;

        // Timer
        struct {
            int32_t si_tid;     // Timer ID
            int32_t si_overrun; // Overrun count
            int32_t si_sigval;  // Signal value
        } _timer;

        // POSIX.1b signals
        struct {
            int32_t  si_pid;    // Sending process ID
            uint32_t si_uid;    // Real user ID of sending process
            int32_t  si_sigval; // Signal value
        } _rt;

        // SIGCHLD
        struct {
            int32_t  si_pid;    // Sending process ID
            uint32_t si_uid;    // Real user ID of sending process
            int32_t  si_status; // Exit value or signal
            int32_t  si_utime;  // User time consumed
            int32_t  si_stime;  // System time consumed
        } _sigchld;

        // SIGILL, SIGFPE, SIGSEGV, SIGBUS
        struct {
            uintptr_t si_addr;     // Faulting instruction or data address
            int32_t   si_addr_lsb; // LSB of the address (if applicable)
        } _sigfault;

        // SIGPOLL
        struct {
            int32_t si_band; // Band event
            int32_t si_fd;   // File descriptor
        } _sigpoll;

        // SIGSYS
        struct {
            uintptr_t si_call_addr; // Calling user insn
            int32_t   si_syscall;   // Number of syscall
            uint32_t  si_arch;      // Architecture
        } _sigsys;
    } _sifields;
} siginfo_t;

struct syscall_regs;
typedef struct process_control_block *pcb_t;

void signal_init();
void do_signal(struct syscall_regs *regs);
int  send_signal_to_process(pcb_t process, int sig);
int  send_signal_to_pgroup(pid_t pgid, int sig);
