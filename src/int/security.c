/**
 * 定义调试模式下的一些保护机制
 */
#include "security.h"
#include "krlibc.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer.h"

uintptr_t __stack_chk_guard = 0;

void init_stack_canary() {
    __stack_chk_guard = sched_clock() ^ 0xDEADBEEF;
    kinfo("stack canary initialized to 0x%llX", (uint64_t)__stack_chk_guard);
}

_Noreturn USED void __stack_chk_fail() {
    logkf("\n***************************************************\n\r");
    logkf("!!! STACK SMASHING DETECTED !!!\n");
    logkf("A stack buffer overflow has corrupted the stack canary.\n\r");
    logkf("***************************************************\n\r");
    disable_scheduler();
    arch_close_interrupt();

    for (;;)
        arch_wait_for_interrupt();
}
