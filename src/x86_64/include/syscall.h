#pragma once

#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084

#define MAX_SYSCALLS         256
#define SYSCALL_SUCCESS      EOK
#define SYSCALL_FAULT        ((uint64_t)-(ENOSYS))
#define SYSCALL_FAULT_(name) ((uint64_t)-(name))

#define syscall_(name)                                                                             \
    uint64_t syscall_##name(                                                                       \
        uint64_t arg0 __attribute__((unused)), uint64_t arg1 __attribute__((unused)),              \
        uint64_t arg2 __attribute__((unused)), uint64_t arg3 __attribute__((unused)),              \
        uint64_t arg4 __attribute__((unused)), uint64_t arg5 __attribute__((unused)),              \
        struct syscall_regs *regs __attribute__((unused)))

// arch_prctl 系统调用 code
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_SET_GS 0x1004
#define ARCH_GET_GS 0x1005

// poll 系统调用标志
#define POLLIN  0x0001 // 有数据可读
#define POLLPRI 0x0002 // 有紧急数据可读（如 socket 的带外数据）
#define POLLOUT 0x0004 // 写操作不会阻塞（可写）

#define POLLERR  0x0008 // 错误（不需要设置，由内核返回）
#define POLLHUP  0x0010 // 挂起（对端关闭）
#define POLLNVAL 0x0020 // fd 无效（文件描述符非法）

// futex 系统调用操作码
#define FUTEX_WAIT        0
#define FUTEX_WAKE        1
#define FUTEX_FD          2
#define FUTEX_REQUEUE     3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP     5
#define FUTEX_LOCK_PI     6
#define FUTEX_UNLOCK_PI   7
#define FUTEX_TRYLOCK_PI  8
#define FUTEX_WAIT_BITSET 9

// stat 文件类型标志
#define S_IFMT   00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

// Linux 兼容层系统调用编号定义
#define SYSCALL_READ        0
#define SYSCALL_WRITE       1
#define SYSCALL_OPEN        2
#define SYSCALL_CLOSE       3
#define SYSCALL_STAT        4
#define SYSCALL_FSTAT       5
#define SYSCALL_LSTAT       6
#define SYSCALL_POLL        7
#define SYSCALL_MMAP        9
#define SYSCALL_MPROTECT    10
#define SYSCALL_MUNMAP      11
/* #define SYSCALL_BRK  12  brk 系统调用不实现*/
#define SYSCALL_SIGACTION   13
#define SYSCALL_RT_SIGMASK  14
#define SYSCALL_SIGRET      15
#define SYSCALL_IOCTL       16
#define SYSCALL_READV       19
#define SYSCALL_WRITEV      20
#define SYSCALL_YIELD       24
#define SYSCALL_MREMAP      25
#define SYSCALL_DUP         32
#define SYSCALL_DUP2        33
#define SYSCALL_NANO_SLEEP  35
#define SYSCALL_GETPID      39
#define SYSCALL_CLONE       56
#define SYSCALL_FORK        57
#define SYSCALL_VFORK       58
#define SYSCALL_EXECVE      59
#define SYSCALL_EXIT        60
#define SYSCALL_WAITPID     61
#define SYSCALL_UNAME       63
#define SYSCALL_FCNTL       72
#define SYSCALL_GETCWD      79
#define SYSCALL_CHDIR       80
#define SYSCALL_SIGALTSTACK 131
#define SYSCALL_PRCTL       157
#define SYSCALL_ARCH_PRCTL  158
#define SYSCALL_G_AFFINITY  160
#define SYSCALL_GET_TID     186
#define SYSCALL_FUTEX       202
#define SYSCALL_SETID_ADDR  218
#define SYSCALL_EXIT_GROUP  231
#define SYSCALL_C_SETTIME   227
#define SYSCALL_C_GETTIME   228
#define SYSCALL_C_GETRES    229
#define SYSCALL_C_NANOSLEEP 230

#include "ctype.h"
#include "krlibc.h"
#include "time.h"

struct syscall_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t func;
    uint64_t errcode;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct pollfd {
    int   fd;
    short events;
    short revents;
};

struct stat {
    long              st_dev;
    unsigned long     st_ino;
    unsigned long     st_nlink;
    int               st_mode;
    int               st_uid;
    int               st_gid;
    long              st_rdev;
    long long         st_size;
    long              st_blksize;
    unsigned long int st_blocks;
    struct timespec   st_atim;
    struct timespec   st_mtim;
    struct timespec   st_ctim;
    char              _pad[24];
};

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                              struct syscall_regs *);

void setup_syscall(bool is_print);
