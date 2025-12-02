#include "syscall.h"
#include "errno.h"
#include "io.h"
#include "nr.h"
#include "term/klog.h"

syscall_t syscall_handlers[MAX_SYSCALLS] = {

};

void syscall_handler(struct pt_regs *regs) {
    uint64_t syscall_id = regs->a7 & 0xFFFFFFFF;

    if (likely(syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL)) {
        csr_set(sstatus, (1UL << 18));
        regs->a0 = (syscall_handlers[syscall_id])(regs->a0, regs->a1, regs->a2, regs->a3, regs->a4,
                                                  regs->a5, regs);
        csr_clear(sstatus, (1UL << 18));
    } else{
        if (unlikely(syscall_id != 12)) logkf("Syscall(%d) cannot implemented.\n", syscall_id);
        regs->a0 = SYSCALL_FAULT_(ENOSYS);
    }
}
