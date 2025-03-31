#pragma once

#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084

#define MAX_SYSCALLS 256

#define syscall_(name) uint64_t syscall_##name(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)

// Cpinl/CP_Kernel 通用系统调用编号定义
#define SYSCALL_PUTC        1 // 向终端put一个字符
#define SYSCALL_PRINT       2 // 向终端put一个字符串 (用于提高运行速度, 而不至于字符串也用putc)
#define SYSCALL_GETCH       3 // 获取该输入缓冲区顶部字符, 如缓冲区为空则阻塞
#define SYSCALL_ALLOC_PAGE  4 // 用户堆扩容系统调用
#define SYSCALL_FREE        5 // 用户堆释放系统调用
#define SYSCALL_EXIT        6 // 终止该进程
#define SYSCALL_GET_ARG     7 // 获取程序实参
#define SYSCALL_POSIX_OPEN  8
#define SYSCALL_POSIX_CLOSE 9
#define SYSCALL_POSIX_READ  10
#define SYSCALL_POSIX_SIZEX 11 // 该调用并不属于POSIX规范
#define SYSCALL_POSIX_WRITE 12
#define SYSCALL_REALLOC     13

#include "ctype.h"

typedef uint64_t (*syscall_t)(uint64_t , uint64_t , uint64_t , uint64_t , uint64_t);

void setup_syscall();
