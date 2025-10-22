#include "errno.h"
#include "syscall.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

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

syscall_(getpid) {
    if (unlikely(arg0 == UINT64_MAX || arg1 == UINT64_MAX || arg2 == UINT64_MAX ||
                 arg3 == UINT64_MAX || arg4 == UINT64_MAX))
        return 1;
    return get_current_task()->process->pid;
}

syscall_(exit_group, int exit_code) {
    pcb_t exit_process = get_current_task()->process;
    logkf("task: Process %s exit with code %d.\n", exit_process->name, exit_code);
    arch_close_interrupt();
    // kill_proc(exit_process, exit_code, true);
    arch_open_interrupt();
    while(true) arch_wait_for_interrupt();
}

syscall_(getuid) {
    return get_current_task()->process->uid;
}

syscall_(yield) {
    scheduler_yield();
    return EOK;
}

syscall_(setpgid, pid_t pid, pid_t pgid) {
    pcb_t process = pid == 0 ? get_current_task()->process : found_pcb(pid);
    if (process == NULL || process->status == T_DEATH) { return SYSCALL_FAULT_(ESRCH); }
    if (pgid == 0) { pgid = process->pgid; }
    process->pgid = pgid;
    return EOK;
}

syscall_(getpgid) {
    size_t pid     = arg0;
    pcb_t  process = pid == 0 ? get_current_task()->process : found_pcb(pid);
    if (process == NULL || process->status == T_DEATH) { return SYSCALL_FAULT_(ESRCH); }
    return process->pgid;
}

syscall_(getppid){
    pcb_t process = get_current_task()->process;
    return process->parent->pid;
}
