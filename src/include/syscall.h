#pragma once

#define MAX_SYSCALLS 256

#define SYSCALL_PUTC 1 // 向终端put一个字符
#define SYSCALL_PRINT 2 // 向终端put一个字符串 (用于提高运行速度, 而不至于字符串也用putc)
#define SYSCALL_GETCH 3 // 获取该输入缓冲区顶部字符, 如缓冲区为空则阻塞
#define SYSCALL_ALLOC_PAGE 4 // 用户堆扩容系统调用
#define SYSCALL_FREE 5 // 用户堆释放系统调用
#define SYSCALL_EXIT 6 // 终止该进程
#define SYSCALL_GET_ARG 7 // 获取程序实参
#define SYSCALL_POSIX_OPEN 8
#define SYSCALL_POSIX_CLOSE 9
#define SYSCALL_POSIX_READ 10

#define __syscall0(id)                                                                             \
  ({                                                                                               \
    ssize_t rets;                                                                                  \
    __asm__ volatile("int $31\n\t" : "=a"(rets) : "0"(id) : "memory", "cc");                           \
    rets;                                                                                          \
  })

#define __syscall1(id, arg1)                                                                       \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    register ssize_t _a1 __asm__("ebx") = __arg1;                                                      \
    __asm__ volatile("int $31\n\t" : "=a"(rets) : "0"(id), "r"(_a1) : "memory", "cc");                 \
    rets;                                                                                          \
  })

#define __syscall2(id, arg1, arg2)                                                                 \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    register ssize_t _a2 __asm__("ecx") = __arg2;                                                      \
    register ssize_t _a1 __asm__("ebx") = __arg1;                                                      \
    __asm__ volatile("int $31\n\t" : "=a"(rets) : "0"(id), "r"(_a1), "r"(_a2) : "memory", "cc");       \
    rets;                                                                                          \
  })

#define __syscall3(id, arg1, arg2, arg3)                                                           \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    ssize_t          __arg3         = (ssize_t)(arg3);                                             \
    register ssize_t _a3 __asm__("edx") = __arg3;                                                      \
    register ssize_t _a2 __asm__("ecx") = __arg2;                                                      \
    register ssize_t _a1 __asm__("ebx") = __arg1;                                                      \
    __asm__ volatile("int $31\n\t"                                                                     \
                 : "=a"(rets)                                                                      \
                 : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3)                                           \
                 : "memory", "cc");                                                                \
    rets;                                                                                          \
  })

#define __syscall4(id, arg1, arg2, arg3, arg4)                                                     \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    ssize_t          __arg3         = (ssize_t)(arg3);                                             \
    ssize_t          __arg4         = (ssize_t)(arg4);                                             \
    register ssize_t _a4 __asm__("esi") = __arg4;                                                      \
    register ssize_t _a3 __asm__("edx") = __arg3;                                                      \
    register ssize_t _a2 __asm__("ecx") = __arg2;                                                      \
    register ssize_t _a1 __asm__("ebx") = __arg1;                                                      \
    __asm__ volatile("int $31\n\t"                                                                     \
                 : "=a"(rets)                                                                      \
                 : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4)                                 \
                 : "memory", "cc");                                                                \
    rets;                                                                                          \
  })

#define __syscall5(id, arg1, arg2, arg3, arg4, arg5)                                               \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    ssize_t          __arg3         = (ssize_t)(arg3);                                             \
    ssize_t          __arg4         = (ssize_t)(arg4);                                             \
    ssize_t          __arg5         = (ssize_t)(arg5);                                             \
    register ssize_t _a5 __asm__("edi") = __arg5;                                                      \
    register ssize_t _a4 __asm__("esi") = __arg4;                                                      \
    register ssize_t _a3 __asm__("edx") = __arg3;                                                      \
    register ssize_t _a2 __asm__("ecx") = __arg2;                                                      \
    register ssize_t _a1 __asm__("ebx") = __arg1;                                                      \
    __asm__ volatile("int $31\n\t"                                                                     \
                 : "=a"(rets)                                                                      \
                 : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5)                       \
                 : "memory", "cc");                                                                \
    rets;                                                                                          \
  })

#define __syscall_concat_x(a, b)                               a##b
#define __syscall_concat(a, b)                                 __syscall_concat_x(a, b)
#define __syscall_argn_private(_0, _1, _2, _3, _4, _5, n, ...) n
#define __syscall_argn(...)                                    __syscall_argn_private(0, ##__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define __syscall(id, ...)                                                                         \
  __syscall_concat(__syscall, __syscall_argn(__VA_ARGS__))(id, ##__VA_ARGS__)

#include "ctypes.h"

typedef uint32_t (*syscall_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void asm_syscall_handler(); //asmfunc.__asm__
void setup_syscall();