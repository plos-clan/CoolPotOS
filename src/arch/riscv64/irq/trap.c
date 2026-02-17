#include "io.h"
#include "metadata.h"
#include "ptrace.h"
#include "rv64_irq.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer_rv64.h"

extern int init_trap_vector(); // vector.S
extern void do_irq(struct pt_regs *regs, uint64_t irq_num);
extern void syscall_handler(struct pt_regs *regs); // syscall.c

void handle_syscall(struct pt_regs *regs) {
    syscall_handler(regs);
}

void handle_exception_c(struct pt_regs *regs, uint64_t cause) {
    switch (cause) {
    case 2: // Illegal instruction
        printk("Illegal instruction at PC: 0x%lx\n", regs->epc);
        break;

    case 3: // Breakpoint
        printk("Breakpoint at PC: 0x%lx\n", regs->epc);
        break;

    case 8: // ecallj
        handle_syscall(regs);
        regs->epc += 4;
        regs->sstatus |= (1UL << 5) | (1UL << 0);
        break;

    case 11: // scall
        handle_syscall(regs);
        regs->epc += 4;
        regs->sstatus |= (1UL << 5) | (1UL << 0);
        break;
    case 12:
        page_fault_(regs, INS_PAGE);
        break;
    case 13:
        page_fault_(regs, LOAD_PAGE);
        break;
    case 15:
        page_fault_(regs, STORE_AMO_PAGE);
        break;
    default:
        printk("Unhandled exception: %lu\n", cause);
        break;
    }
}

void handle_interrupt_c(struct pt_regs *regs, uint64_t cause) {
    switch (cause) {
    case 5: // timer interrupt
        riscv64_timer_handler(regs);
        sbi_set_timer(get_timer() + timer_freq / SCHED_TIMER_SPEED);
        scheduler_handler(0, NULL, regs);
        break;
    default:
        do_irq(regs, cause);
        break;
    }
}

void handle_trap_c(struct pt_regs *regs) {
    uint64_t is_interrupt = csr_read(scause) & (1UL << 63);
    uint64_t cause_code   = csr_read(scause) & 0x7FFFFFFFFFFFFFFF;

    if (is_interrupt) {
        handle_interrupt_c(regs, cause_code);
    } else {
        if (cause_code != 8 && cause_code != 11) {
            printk("Exception occurred:\n");

            // dump_registers(regs);
        }

        handle_exception_c(regs, cause_code);
    }
}

int trap_init(void) {
    // 调用汇编初始化函数
    int result = init_trap_vector();
    if (result != 0) {
        printk("Failed to initialize trap vector: %d\n", result);
        return result;
    }

    return 0;
}
