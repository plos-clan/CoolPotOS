#pragma once

#define MAX_SYSCALLS 500

typedef uint64_t (*syscall_t)(
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6,
    struct pt_regs *regs
);

#define SYSCALL_IOCTL           29
#define SYSCALL_MOUNT           40
#define SYSCALL_OPENAT          56
#define SYSCALL_CLOSE           57
#define SYSCALL_PIPE2           59
#define SYSCALL_LSEEK           62
#define SYSCALL_READ            63
#define SYSCALL_WRITE           64
#define SYSCALL_READV           65
#define SYSCALL_WRITEV          66
#define SYSCALL_PREAD           67
#define SYSCALL_PWRITE          68
#define SYSCALL_PREADV          69
#define SYSCALL_PWRITEV         70
#define SYSCALL_EXIT            93
#define SYSCALL_EXIT_GROUP      94
#define SYSCALL_NANOSLEEP       101
#define SYSCALL_SYSLOG          116
#define SYSCALL_SCHED_YIELD     124
#define SYSCALL_REBOOT          142
#define SYSCALL_GETRLIMIT       163
#define SYSCALL_GETPID          172
#define SYSCALL_GETPPID         173
#define SYSCALL_MUNMAP          215
#define SYSCALL_MREMAP          216
#define SYSCALL_MMAP            222
#define SYSCALL_PRLIMIT64       261
#define SYSCALL_COPY_FILE_RANGE 295
#define SYSCALL_FUTEX           422
