#include "io.h"
#include "krlibc.h"
#include "ptrace.h"
#include "sbi.h"
#include "timer_rv64.h"

uint64_t timer_freq = TIMER_FREQ;

uint64_t get_timer(void) {
    uint64_t time;
    __asm__ volatile(".option push\n"
                     ".option norvc\n"
                     "rdtime %0\n"
                     ".option pop\n"
                     : "=r"(time));
    return time;
}

void sbi_set_timer(uint64_t stime_value) {
    sbi_ecall(SBI_SET_TIMER, 0, stime_value, 0, 0, 0, 0, 0);
}

void timer_init_hart(uint32_t hart_id) {
    /* 使能S模式定时器中断 */
    csr_set(sie, (1 << 5)); /* STIE */

    //    uacpi_table  rhct_table;
    //    uacpi_status status = uacpi_table_find_by_signature(ACPI_RHCT_SIGNATURE, &rhct_table);
    //    if (status == UACPI_STATUS_OK) {
    //        struct acpi_rhct *rhct = rhct_table.ptr;
    //        timer_freq             = rhct->timebase_frequency;
    //    }

    arch_open_interrupt();

    sbi_set_timer(get_timer() + timer_freq / SCHED_TIMER_SPEED);
}

void riscv64_timer_handler(struct pt_regs *regs) {
    // sched_check_wakeup();
}

uint64_t nano_time() {
    return get_timer() * (timer_freq / 100000);
}
