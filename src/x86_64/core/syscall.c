#include "syscall.h"
#include "io.h"
#include "kprint.h"
#include "scheduler.h"

extern void asm_syscall_entry();

static inline void enable_syscall() {
    uint64_t efer;
    efer  = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
}

syscall_(putc){
    printk("%c",(char)arg1);
    return 0;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = syscall_putc,
};

registers_t *syscall_handle(registers_t *reg) {
    if (0 <= reg->rax && reg->rax < MAX_SYSCALLS && syscall_handlers[reg->rax] != NULL) {
        reg->rax = ((syscall_t)syscall_handlers[reg->rax])(reg->rbx, reg->rcx, reg->rdx, reg->rsi, reg->rdi);
    } else reg->rax = -1;
    return reg;
}

void setup_syscall() {
    //enable_syscall();
    register_interrupt_handler(0x80,asm_syscall_entry,0,0x8E | 0x60);
    kinfo("Setup cpinl syscall table.");
}
