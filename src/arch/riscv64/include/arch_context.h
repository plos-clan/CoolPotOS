#pragma once

#include "ptrace.h"

typedef struct fpu_context {
    uint64_t regs[32];
    uint64_t fcsr;
} fpu_context_t;

struct arch_context_ {
    uint64_t       ra;
    uint64_t       sp;
    struct pt_regs ctx;
    fpu_context_t  fpu_ctx;
};
