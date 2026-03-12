#pragma once

#include "fpu.h"
#include "ptrace.h"

struct arch_context_ {
    struct pt_regs regs;
    fpu_context_t *context;
    uint64_t kernel_stack;
    uint64_t user_stack;
    uint64_t user_stack_top;
    uint64_t fs, gs;
    uint64_t fs_base, gs_base;
};

typedef void (*arch_stack_entry_t)(void *);

void arch_run_on_kernel_stack(uint64_t stack_top, arch_stack_entry_t entry, void *arg) __attribute__((noreturn));
