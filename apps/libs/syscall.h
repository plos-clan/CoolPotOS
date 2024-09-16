#pragma once

#define __syscall0(id)                                                                             \
  ({                                                                                               \
    ssize_t rets;                                                                                  \
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(id) : "memory", "cc");                           \
    rets;                                                                                          \
  })

#define __syscall1(id, arg1)                                                                       \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    register ssize_t _a1 asm("ebx") = __arg1;                                                      \
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(id), "r"(_a1) : "memory", "cc");                 \
    rets;                                                                                          \
  })

#define __syscall2(id, arg1, arg2)                                                                 \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    register ssize_t _a2 asm("ecx") = __arg2;                                                      \
    register ssize_t _a1 asm("ebx") = __arg1;                                                      \
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(id), "r"(_a1), "r"(_a2) : "memory", "cc");       \
    rets;                                                                                          \
  })

#define __syscall3(id, arg1, arg2, arg3)                                                           \
  ({                                                                                               \
    ssize_t          rets;                                                                         \
    ssize_t          __arg1         = (ssize_t)(arg1);                                             \
    ssize_t          __arg2         = (ssize_t)(arg2);                                             \
    ssize_t          __arg3         = (ssize_t)(arg3);                                             \
    register ssize_t _a3 asm("edx") = __arg3;                                                      \
    register ssize_t _a2 asm("ecx") = __arg2;                                                      \
    register ssize_t _a1 asm("ebx") = __arg1;                                                      \
    asm volatile("int $31\n\t"                                                                     \
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
    register ssize_t _a4 asm("esi") = __arg4;                                                      \
    register ssize_t _a3 asm("edx") = __arg3;                                                      \
    register ssize_t _a2 asm("ecx") = __arg2;                                                      \
    register ssize_t _a1 asm("ebx") = __arg1;                                                      \
    asm volatile("int $31\n\t"                                                                     \
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
    register ssize_t _a5 asm("edi") = __arg5;                                                      \
    register ssize_t _a4 asm("esi") = __arg4;                                                      \
    register ssize_t _a3 asm("edx") = __arg3;                                                      \
    register ssize_t _a2 asm("ecx") = __arg2;                                                      \
    register ssize_t _a1 asm("ebx") = __arg1;                                                      \
    asm volatile("int $31\n\t"                                                                     \
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
