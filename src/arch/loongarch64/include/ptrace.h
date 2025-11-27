#pragma once

#include "types.h"

struct pt_regs {
    uint64_t r0;  // 0*8
    uint64_t ra;  // 1*8
    uint64_t tp;  // 2*8
    uint64_t usp; // 3*8 (user stack pointer)
    uint64_t a0;  // 4*8
    uint64_t a1;  // 5*8
    uint64_t a2;  // 6*8
    uint64_t a3;  // 7*8
    uint64_t a4;  // 8*8
    uint64_t a5;  // 9*8
    uint64_t a6;  // 10*8
    uint64_t a7;  // 11*8
    uint64_t t0;  // 12*8
    uint64_t t1;  // 13*8
    uint64_t t2;  // 14*8
    uint64_t t3;  // 15*8
    uint64_t t4;  // 16*8
    uint64_t t5;  // 17*8
    uint64_t t6;  // 18*8
    uint64_t t7;  // 19*8
    uint64_t t8;  // 20*8
    uint64_t r21; // 21*8
    uint64_t fp;  // 22*8
    uint64_t s0;  // 23*8
    uint64_t s1;  // 24*8
    uint64_t s2;  // 25*8
    uint64_t s3;  // 26*8
    uint64_t s4;  // 27*8
    uint64_t s5;  // 28*8
    uint64_t s6;  // 29*8
    uint64_t s7;  // 30*8
    uint64_t s8;  // 31*8
    /// original syscall arg0
    uint64_t orig_a0;

    uint64_t csr_era;
    uint64_t csr_badvaddr;
    uint64_t csr_crmd;
    uint64_t csr_prmd;
    uint64_t csr_euen;
    uint64_t csr_ecfg;
    uint64_t csr_estat;
};

struct syscall_regs {
    struct pt_regs regs;
};
