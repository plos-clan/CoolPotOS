#pragma once

#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084

#define MAX_SYSCALLS    256
#define SYSCALL_SUCCESS 0
#define SYSCALL_FAULT   ((uint64_t)-(ENOSYS))

#define syscall_(name)                                                                             \
    uint64_t syscall_##name(                                                                       \
        uint64_t arg0 __attribute__((unused)), uint64_t arg1 __attribute__((unused)),              \
        uint64_t arg2 __attribute__((unused)), uint64_t arg3 __attribute__((unused)),              \
        uint64_t arg4 __attribute__((unused)), uint64_t arg5 __attribute__((unused)))

// arch_prctl 系统调用 code
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_SET_GS 0x1004
#define ARCH_GET_GS 0x1005

// plos-clan 通用系统调用编号定义
#define SYSCALL_EXIT       0
#define SYSCALL_ABORT      1
#define SYSCALL_MMAP       2
#define SYSCALL_SIGRET     5
#define SYSCALL_SIGNAL     6
#define SYSCALL_WAITPID    7
#define SYSCALL_OPEN       8
#define SYSCALL_CLOSE      9
#define SYSCALL_READ       10
#define SYSCALL_SIZE       11
#define SYSCALL_WRITE      12
#define SYSCALL_GETPID     13
#define SYSCALL_PRCTL      14
#define SYSCALL_CLONE      15
#define SYSCALL_ARCH_PRCTL 16
#define SYSCALL_YIELD      17
#define SYSCALL_IOCTL      54
#define SYSCALL_UNAME      63
#define SYSCALL_NANO_SLEEP 162

#include "ctype.h"

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

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void setup_syscall();
