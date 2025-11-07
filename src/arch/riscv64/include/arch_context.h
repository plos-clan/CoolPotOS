#pragma once

#include "ptrace.h"

struct arch_context_ {
    struct pt_regs regs;
    uint64_t       kernel_stack;
    uint64_t       user_stack;
    uint64_t       user_stack_top;
};
