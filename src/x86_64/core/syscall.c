#include "syscall.h"
#include "io.h"
#include "krlibc.h"
#include "kprint.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pcb.h"

extern void asm_syscall_entry();

static inline void enable_syscall() {
    uint64_t efer;
    efer = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
}

syscall_(putc) {
    char c = *((char *) arg0);
    printk("%c", c);
    return 0;
}

syscall_(print) {
    printk("%s", (char *) arg0);
    return 0;
}

syscall_(getch) {
    return kernel_getch();
}

syscall_(exit) {
    int exit_code = arg0;
    get_current_task()->status = DEATH;
    cpu_hlt;
    return 0;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = syscall_putc,
        [SYSCALL_PRINT] = syscall_print,
        [SYSCALL_GETCH] = syscall_getch,
        [SYSCALL_EXIT] = syscall_exit,
};

registers_t *syscall_handle(registers_t *reg) {
    if (0 <= reg->rax && reg->rax < MAX_SYSCALLS && syscall_handlers[reg->rax] != NULL) {
        reg->rax = ((syscall_t) syscall_handlers[reg->rax])(reg->rbx, reg->rcx, reg->rdx, reg->rsi, reg->rdi);
    } else reg->rax = -1;
    return reg;
}

void setup_syscall() {
    //enable_syscall();
    register_interrupt_handler(0x80, asm_syscall_entry, 0, 0x8E | 0x60);
    kinfo("Setup cpinl syscall table.");
}
