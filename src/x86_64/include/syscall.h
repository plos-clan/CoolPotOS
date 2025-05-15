#pragma once

#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084

#define MAX_SYSCALLS    256
#define SYSCALL_SUCCESS 0
#define SYSCALL_FAULT   (-1)

#define syscall_(name)                                                                             \
    uint64_t syscall_##name(                                                                       \
        uint64_t arg0 __attribute__((unused)), uint64_t arg1 __attribute__((unused)),              \
        uint64_t arg2 __attribute__((unused)), uint64_t arg3 __attribute__((unused)),              \
        uint64_t arg4 __attribute__((unused)))

// plos-clan 通用系统调用编号定义
#define SYSCALL_EXIT    0
#define SYSCALL_ABORT   1
#define SYSCALL_MMAP    2
#define SYSCALL_SIGRET  5
#define SYSCALL_SIGNAL  6
#define SYSCALL_WAITPID 7
#define SYSCALL_OPEN    8
#define SYSCALL_CLOSE   9
#define SYSCALL_READ    10
#define SYSCALL_SIZE    11
#define SYSCALL_WRITE   12
#define SYSCALL_GETPID  13
#define SYSCALL_PRCTL   14

#include "ctype.h"

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void setup_syscall();
