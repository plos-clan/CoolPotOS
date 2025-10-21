#pragma once

#include "ptrace.h"

// 一个非常取巧的宏魔法, 可以简化 syscall 函数的定义
#define __EXPAND_PARAMS(...) __VA_ARGS__
#define __CONCAT_IMPL(a, b)  a##b
#define __CONCAT(a, b)       __CONCAT_IMPL(a, b)

#define __ARGS_COUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, N, ...) N

#define __ARGS_COUNT(...) __EXPAND_PARAMS(__ARGS_COUNT_IMPL(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0))

#define __SYSCALL_IMPL_0(NAME)                                                                     \
    uint64_t syscall_##NAME(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,            \
                            uint64_t arg4, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_1(NAME, P1)                                                                 \
    uint64_t syscall_##NAME(P1, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,        \
                            uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_2(NAME, P1, P2)                                                             \
    uint64_t syscall_##NAME(P1, P2, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,    \
                            struct syscall_regs *regs)

#define __SYSCALL_IMPL_3(NAME, P1, P2, P3)                                                         \
    uint64_t syscall_##NAME(P1, P2, P3, uint64_t arg3, uint64_t arg4, uint64_t arg5,               \
                            struct syscall_regs *regs)

#define __SYSCALL_IMPL_4(NAME, P1, P2, P3, P4)                                                     \
    uint64_t syscall_##NAME(P1, P2, P3, P4, uint64_t arg4, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_5(NAME, P1, P2, P3, P4, P5)                                                 \
    uint64_t syscall_##NAME(P1, P2, P3, P4, P5, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_6(NAME, P1, P2, P3, P4, P5, P6)                                             \
    uint64_t syscall_##NAME(P1, P2, P3, P4, P5, P6, struct syscall_regs *regs)

#define __SYSCALL_DISPATCH(N, NAME, ...) __CONCAT(__SYSCALL_IMPL_, N)(NAME, ##__VA_ARGS__)

#define syscall_(NAME, ...) __SYSCALL_DISPATCH(__ARGS_COUNT(0, ##__VA_ARGS__), NAME, ##__VA_ARGS__)

#define syscall_def_(name)                                                                         \
    uint64_t syscall_##name(                                                                       \
        uint64_t arg0 __attribute__((unused)), uint64_t arg1 __attribute__((unused)),              \
        uint64_t arg2 __attribute__((unused)), uint64_t arg3 __attribute__((unused)),              \
        uint64_t arg4 __attribute__((unused)), uint64_t arg5 __attribute__((unused)),              \
        struct syscall_regs *regs __attribute__((unused)))

#define SYSCALL_FAULT_(name) ((uint64_t)-(name))

#include "types.h"

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct timespec {
    uint64_t tv_sec;
    uint64_t tv_nsec;
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

void arch_enable_syscall();

// fs syscall
syscall_(open, char *path0, uint64_t flags, uint64_t mode);
syscall_(close, int fd);
syscall_(write, int fd, uint8_t *buffer, size_t size);
syscall_(read, int fd, uint8_t *buffer, size_t size);
syscall_(writev, int fd, struct iovec *iov, int iovcnt);
syscall_(readv, int fd, struct iovec *iov, int iovcnt0);

// proc syscall
syscall_(exit, int exit_code);
syscall_(set_tid_address, int *tidptr);
