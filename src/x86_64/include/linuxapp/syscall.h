
#define asm __asm__

#define __syscall0(id)                                                                             \
    ({                                                                                             \
        __INTPTR_TYPE__ rets;                                                                      \
        asm volatile("syscall\n\t" : "=a"(rets) : "0"(id) : "memory", "cc", "r11", "cx");          \
        rets;                                                                                      \
    })

#define __syscall1(id, arg1)                                                                       \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1)                                                           \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall2(id, arg1, arg2)                                                                 \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        __INTPTR_TYPE__          __arg2         = (__INTPTR_TYPE__)(arg2);                         \
        register __INTPTR_TYPE__ _a2 asm("rsi") = __arg2;                                          \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1), "r"(_a2)                                                 \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall3(id, arg1, arg2, arg3)                                                           \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        __INTPTR_TYPE__          __arg2         = (__INTPTR_TYPE__)(arg2);                         \
        __INTPTR_TYPE__          __arg3         = (__INTPTR_TYPE__)(arg3);                         \
        register __INTPTR_TYPE__ _a3 asm("rdx") = __arg3;                                          \
        register __INTPTR_TYPE__ _a2 asm("rsi") = __arg2;                                          \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3)                                       \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall4(id, arg1, arg2, arg3, arg4)                                                     \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        __INTPTR_TYPE__          __arg2         = (__INTPTR_TYPE__)(arg2);                         \
        __INTPTR_TYPE__          __arg3         = (__INTPTR_TYPE__)(arg3);                         \
        __INTPTR_TYPE__          __arg4         = (__INTPTR_TYPE__)(arg4);                         \
        register __INTPTR_TYPE__ _a4 asm("r10") = __arg4;                                          \
        register __INTPTR_TYPE__ _a3 asm("rdx") = __arg3;                                          \
        register __INTPTR_TYPE__ _a2 asm("rsi") = __arg2;                                          \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4)                             \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall5(id, arg1, arg2, arg3, arg4, arg5)                                               \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        __INTPTR_TYPE__          __arg2         = (__INTPTR_TYPE__)(arg2);                         \
        __INTPTR_TYPE__          __arg3         = (__INTPTR_TYPE__)(arg3);                         \
        __INTPTR_TYPE__          __arg4         = (__INTPTR_TYPE__)(arg4);                         \
        __INTPTR_TYPE__          __arg5         = (__INTPTR_TYPE__)(arg5);                         \
        register __INTPTR_TYPE__ _a5 asm("r8")  = __arg5;                                          \
        register __INTPTR_TYPE__ _a4 asm("r10") = __arg4;                                          \
        register __INTPTR_TYPE__ _a3 asm("rdx") = __arg3;                                          \
        register __INTPTR_TYPE__ _a2 asm("rsi") = __arg2;                                          \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5)                   \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall6(id, arg1, arg2, arg3, arg4, arg5, arg6)                                         \
    ({                                                                                             \
        __INTPTR_TYPE__          rets;                                                             \
        __INTPTR_TYPE__          __arg1         = (__INTPTR_TYPE__)(arg1);                         \
        __INTPTR_TYPE__          __arg2         = (__INTPTR_TYPE__)(arg2);                         \
        __INTPTR_TYPE__          __arg3         = (__INTPTR_TYPE__)(arg3);                         \
        __INTPTR_TYPE__          __arg4         = (__INTPTR_TYPE__)(arg4);                         \
        __INTPTR_TYPE__          __arg5         = (__INTPTR_TYPE__)(arg5);                         \
        __INTPTR_TYPE__          __arg6         = (__INTPTR_TYPE__)(arg6);                         \
        register __INTPTR_TYPE__ _a6 asm("r9")  = __arg6;                                          \
        register __INTPTR_TYPE__ _a5 asm("r8")  = __arg5;                                          \
        register __INTPTR_TYPE__ _a4 asm("r10") = __arg4;                                          \
        register __INTPTR_TYPE__ _a3 asm("rdx") = __arg3;                                          \
        register __INTPTR_TYPE__ _a2 asm("rsi") = __arg2;                                          \
        register __INTPTR_TYPE__ _a1 asm("rdi") = __arg1;                                          \
        asm volatile("syscall\n\t"                                                                 \
                     : "=a"(rets)                                                                  \
                     : "0"(id), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6)         \
                     : "memory", "cc", "r11", "cx");                                               \
        rets;                                                                                      \
    })

#define __syscall_concat_x(a, b)                                   a##b
#define __syscall_concat(a, b)                                     __syscall_concat_x(a, b)
#define __syscall_argn_private(_0, _1, _2, _3, _4, _5, _6, n, ...) n
#define __syscall_argn(...)                                        __syscall_argn_private(0, ##__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)
#define __syscall(id, ...)                                                                         \
    __syscall_concat(__syscall, __syscall_argn(__VA_ARGS__))(id, ##__VA_ARGS__)

#define SYS_fork    57
#define SYS_execve  59
#define SYS_wait4   61
#define SYS_getcwd 79
