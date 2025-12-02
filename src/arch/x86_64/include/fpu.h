#pragma once

#include "types.h"

typedef struct fpu_context {
    uint16_t fcw;
    uint16_t fsw;
    uint16_t ftw;
    uint16_t fop;
    uint64_t word2;
    uint64_t word3;
    uint32_t mxscr;
    uint32_t mxcsr_mask;
    uint64_t mm[16];
    uint64_t xmm[32];
    uint64_t rest[12];
} __attribute__((aligned(16))) fpu_context_t;

void save_fpu_context(fpu_context_t *ctx);
void restore_fpu_context(fpu_context_t *ctx);
void float_processor_setup();