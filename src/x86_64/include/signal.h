#pragma once

#define EPERM   1  // Operation not permitted
#define ENOENT  2  // No such file or directory
#define ESRCH   3  // No such process
#define EINTR   4  // Interrupted system call
#define EIO     5  // I/O error
#define ENXIO   6  // No such device or address
#define E2BIG   7  // Argument list too long
#define ENOEXEC 8  // Exec format error
#define EBADF   9  // Bad file descriptor
#define ECHILD  10 // No child processes
#define EAGAIN  11 // Try again / Resource temporarily unavailable
#define ENOMEM  12 // Out of memory
#define EACCES  13 // Permission denied
#define EFAULT  14 // Bad address
#define EBUSY   16 // Device or resource busy
#define EEXIST  17 // File exists
#define EXDEV   18 // Cross-device link
#define ENODEV  19 // No such device
#define ENOTDIR 20 // Not a directory
#define EISDIR  21 // Is a directory
#define EINVAL  22 // Invalid argument
#define ENFILE  23 // File table overflow
#define EMFILE  24 // Too many open files
#define ENOSPC  28 // No space left on device
#define ESPIPE  29 // Illegal seek
#define EROFS   30 // Read-only file system
#define EMLINK  31 // Too many links
#define EPIPE   32 // Broken pipe
#define EDOM    33 // Math argument out of domain
#define ERANGE  34 // Math result not representable

#define SIGHUP  1  // Hangup (终端断开)
#define SIGINT  2  // Interrupt (Ctrl+C)
#define SIGQUIT 3  // Quit (Ctrl+\)
#define SIGILL  4  // Illegal instruction
#define SIGTRAP 5  // Trace trap
#define SIGABRT 6  // Abort
#define SIGBUS  7  // Bus error
#define SIGFPE  8  // Floating-point exception
#define SIGKILL 9  // Kill signal (不可捕获/屏蔽)
#define SIGUSR1 10 // User-defined signal 1
#define SIGSEGV 11 // Segmentation fault
#define SIGUSR2 12 // User-defined signal 2
#define SIGPIPE 13 // Broken pipe
#define SIGALRM 14 // Alarm clock
#define SIGTERM 15 // Termination signal
#define SIGCHLD 17 // Child stopped or terminated
#define SIGCONT 18 // Continue if stopped
#define SIGSTOP 19 // Stop process (不可捕获)
#define SIGTSTP 20 // Stop typed at terminal (Ctrl+Z)
#define SIGTTIN 21 // Background read from tty
#define SIGTTOU 22 // Background write to tty

#include "ctype.h"
#include "krlibc.h"
#include "scheduler.h"

typedef struct signal_block signal_block_t;

struct signal_block {
    uint64_t pending_signals;         // 用 bitmap 表示待处理信号
    void (*signal_handlers[64])(int); // 每个信号对应的用户处理器
    bool signal_mask[64];             // 屏蔽的信号
} __attribute__((packed));

struct sigframe {
    uint64_t    ret_addr;
    int         sig;
    TaskContext context0; // 信号上下文
};

typedef struct thread_control_block  *tcb_t;
typedef struct process_control_block *pcb_t;

int  send_signal(int pid, int sig);
void setup_signal_stack(tcb_t thread, int signum, void *handler);
void register_signal(pcb_t task, int sig, void (*handler)(int));
void check_pending_signals(pcb_t proc, tcb_t thread);
