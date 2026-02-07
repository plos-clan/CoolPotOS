#pragma once

#if defined(__x86_64__) || defined(__amd64__)
#    include "fpu.h"
#endif
#include "ptrace.h"
#include "task/task.h"

#define SIGNAL_FRAME_MAGIC 0x4350534947464D00ULL

struct signal_frame {
    uint64_t pretcode;
    uint32_t signum;
    uint32_t _pad0;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t rax;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t saved_blocked;
    uint64_t saved_call_in_signal;
#if defined(__x86_64__) || defined(__amd64__)
    fpu_context_t fpu_state;
#endif
    uint64_t magic;
    uint8_t  trampoline[16];
};

bool     arch_signal_setup(tcb_t task, int signum, sigaction_t *action, struct syscall_regs *regs);
uint64_t arch_signal_sigreturn(struct syscall_regs *regs);
