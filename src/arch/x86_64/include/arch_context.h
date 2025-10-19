#pragma once

#include "fpu.h"
#include "ptrace.h"

struct arch_context_ {
    uint64_t       syscall_stack;      // 系统调用栈顶地址
    uint64_t       syscall_stack_user; // 用户态下系统调用栈缓存
    uint64_t       signal_stack;       // 信号栈顶地址
    uint64_t       call_in_signal;     // 是否在信号处理过程
    struct pt_regs regs;
    fpu_context_t  context;
    uint64_t       kernel_stack;
    uint64_t       user_stack;
    uint64_t       user_stack_top;
    uint64_t       fs, gs;
    uint64_t       fs_base, gs_base;
};
