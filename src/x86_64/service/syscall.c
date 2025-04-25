#include "syscall.h"
#include "io.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"
#include "scheduler.h"

extern void asm_syscall_entry();

extern lock_queue *pgb_queue;

static inline void enable_syscall() {
    uint64_t efer;
    efer  = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
}

syscall_(exit) {
    int exit_code = arg0;
    logkf("Process %s exit with code %d.\n", get_current_task()->parent_group->name, exit_code);
    kill_proc(get_current_task()->parent_group);
    cpu_hlt;
    return 0;
}

syscall_(abort) {
    logkf("Process %s abort.\n", get_current_task()->parent_group->name);
    kill_proc(get_current_task()->parent_group);
    cpu_hlt;
    return 0;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT]  = syscall_exit,
    [SYSCALL_ABORT] = syscall_abort,
};

registers_t *syscall_handle(registers_t *reg) {
    size_t syscall_id = reg->rax;
    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        reg->rax = ((syscall_t)syscall_handlers[syscall_id])(reg->rbx, reg->rcx, reg->rdx, reg->rsi,
                                                             reg->rdi);
    }
    return reg;
}

void setup_syscall() {
    UNUSED(enable_syscall);
    register_interrupt_handler(0x80, asm_syscall_entry, 0, 0x8E | 0x60);
    kinfo("Setup CP_Kernel syscall table.");
}
