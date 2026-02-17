/**
 * 定义调试模式下的一些保护机制
 */
#include "security.h"
#include "krlibc.h"
#include "metadata.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer.h"

#if defined(__x86_64__) || defined(__amd64__)
#    include "fsgsbase.h"
#endif

static struct pthread pthread_self;

uint64_t __stack_chk_guard;

void test_stack_chk(void) {
    int val = 1234;
    *(long *)&val = 5678;
}

__attr(always_inline) void init_stack_canary(void) {
    __stack_chk_guard = (uint64_t)&__stack_chk_guard * 1103515245;
    pthread_self.self = &pthread_self;
    pthread_self.canary = __stack_chk_guard;
    uint64_t chk_base_value = (uint64_t)&pthread_self;
#if defined(__x86_64__) || defined(__amd64__)
    __asm__ volatile("wrmsr" : : "c"(IA32_FS_BASE), "a"(chk_base_value), "d"(chk_base_value >> 32));
#endif
}

_Noreturn USED void __stack_chk_fail() {
    logkf("\n***************************************************\n\r");
    logkf("!!! STACK SMASHING DETECTED !!!\n");
    logkf("A stack buffer overflow has corrupted the stack canary.\n\r");
    logkf("***************************************************\n\r");
    scheduler_disable();
    arch_close_interrupt();

    for (;;)
        arch_wait_for_interrupt();
}
