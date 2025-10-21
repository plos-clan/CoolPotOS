#include "syscall.h"
#include "task/task.h"
#include "term/klog.h"
#include "errno.h"

syscall_(exit, int exit_code) {
    tcb_t exit_thread = get_current_task();
    logkf("sys_exit: Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    //TODO kill_thread(exit_thread);
    printk("EXIT PR\n");
    arch_open_interrupt();
    while (true) arch_wait_for_interrupt();
    return EOK;
}

syscall_(set_tid_address, int *tidptr) {
    if (unlikely(tidptr == NULL)) return SYSCALL_FAULT_(EINVAL);
    tcb_t thread          = get_current_task();
    thread->tid_address   = (uint64_t)tidptr;
    thread->tid_directory = get_current_directory();
    return EOK;
}
