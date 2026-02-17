#pragma once

#define MAX_SYSCALLS 500

typedef uint64_t (*syscall_t)(
    uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6,
    struct pt_regs *regs
);
