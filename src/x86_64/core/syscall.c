#include "syscall.h"
#include "kprint.h"
#include "io.h"
#include "scheduler.h"
#include "klog.h"

extern void asm_syscall_entry();

static inline void enable_syscall(){
    uint64_t efer;
    efer = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);


}

registers_t *syscall_handle(registers_t *reg){
    logkf("Syscall id: %d\n",reg->rax);
    return reg;
}

void setup_syscall(){
    //enable_syscall();
    //register_interrupt_handler(0x80,asm_syscall_entry,0,0x8E | 0x60);
    kinfo("Setup cpinl syscall table.");
}
