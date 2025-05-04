#include "syscall.h"
#include "io.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"
#include "scheduler.h"
#include "vfs.h"

extern void asm_syscall_entry();

extern lock_queue *pgb_queue;

static inline void enable_syscall() {
    uint64_t efer;
    efer  = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
}

syscall_(exit) {
    int exit_code = arg0;
    logkf("Process %s exit with code %d.\n", get_current_task()->parent_group->name, exit_code);
    kill_proc(get_current_task()->parent_group, exit_code);
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(abort) {
    logkf("Process %s abort.\n", get_current_task()->parent_group->name);
    kill_proc(get_current_task()->parent_group, -1);
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(open) {
    char *path = (char *)arg0;
    if (path == NULL) return -1;
    vfs_node_t node  = vfs_open(path);
    int        index = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, node);
    spin_unlock(get_current_task()->parent_group->file_open->lock);
    return index;
}

syscall_(close) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT;
    vfs_node_t node = (vfs_node_t)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_free(node);
    return SYSCALL_SUCCESS;
}

syscall_(write) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    return vfs_write(node, buffer, 0, arg2);
}

syscall_(read) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    int        ret    = vfs_read(node, buffer, 0, arg2);
    return ret;
}

syscall_(waitpid) {
    int pid = arg0;
    if (pid < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (found_pcb(pid) == NULL) return SYSCALL_FAULT;
    int *status = (int *)arg1;
    *status     = waitpid(pid);
    return SYSCALL_SUCCESS;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT] = syscall_exit,       [SYSCALL_ABORT] = syscall_abort,
    [SYSCALL_OPEN] = syscall_open,       [SYSCALL_CLOSE] = syscall_close,
    [SYSCALL_WRITE] = syscall_write,     [SYSCALL_READ] = syscall_read,
    [SYSCALL_WAITPID] = syscall_waitpid,
};

registers_t *syscall_handle(registers_t *reg) {
    open_interrupt;
    size_t syscall_id = reg->rax;
    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        reg->rax = ((syscall_t)syscall_handlers[syscall_id])(reg->rbx, reg->rcx, reg->rdx, reg->rsi,
                                                             reg->rdi);
    }
    return reg;
}

void setup_syscall() {
    UNUSED(enable_syscall);
    register_interrupt_handler(0x80, asm_syscall_entry, 0, 0x8E | 0x60);
    kinfo("Setup CP_Kernel syscall table.");
}
