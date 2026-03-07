#include "syscall.h"
#include "errno.h"
#include "io.h"
#include "nr.h"
#include "term/klog.h"

static syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_READ]            = (syscall_t)syscall_read,
    [SYSCALL_WRITE]           = (syscall_t)syscall_write,
    [SYSCALL_CLOSE]           = (syscall_t)syscall_close,
    [SYSCALL_IOCTL]           = (syscall_t)syscall_ioctl,
    [SYSCALL_LSEEK]           = (syscall_t)syscall_lseek,
    [SYSCALL_READV]           = (syscall_t)syscall_readv,
    [SYSCALL_WRITEV]          = (syscall_t)syscall_writev,
    [SYSCALL_PREAD]           = (syscall_t)syscall_pread,
    [SYSCALL_PWRITE]          = (syscall_t)syscall_pwrite,
    [SYSCALL_OPENAT]          = (syscall_t)syscall_open,
    [SYSCALL_EXIT]            = (syscall_t)syscall_exit,
    [SYSCALL_EXIT_GROUP]      = (syscall_t)syscall_exit_group,
    [SYSCALL_MOUNT]           = (syscall_t)syscall_mount,
    [SYSCALL_PIPE2]           = (syscall_t)syscall_pipe2,
    [SYSCALL_REBOOT]          = (syscall_t)syscall_reboot,
    [SYSCALL_GETPID]          = (syscall_t)syscall_getpid,
    [SYSCALL_GETPPID]         = (syscall_t)syscall_getppid,
    [SYSCALL_MUNMAP]          = (syscall_t)syscall_munmap,
    [SYSCALL_MREMAP]          = (syscall_t)syscall_mremap,
    [SYSCALL_MMAP]            = (syscall_t)syscall_mmap,
    [SYSCALL_FUTEX]           = (syscall_t)syscall_futex,
    [SYSCALL_GETRLIMIT]       = (syscall_t)syscall_get_rlimit,
    [SYSCALL_PRLIMIT64]       = (syscall_t)syscall_prlimit64,
    [SYSCALL_COPY_FILE_RANGE] = (syscall_t)syscall_copy_file_range,
    [SYSCALL_SYSLOG]          = (syscall_t)syscall_sys_log,
    [SYSCALL_SCHED_YIELD]     = (syscall_t)syscall_yield,
    [SYSCALL_NANOSLEEP]       = (syscall_t)syscall_nano_sleep,
};

void syscall_handler(struct pt_regs *regs) {
    uint64_t syscall_id = regs->a7 & 0xFFFFFFFF;

    if (likely(syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL)) {
        csr_set(sstatus, 1UL << 18);
        regs->a0 = syscall_handlers[syscall_id](
            regs->a0, regs->a1, regs->a2, regs->a3, regs->a4, regs->a5, regs
        );
        csr_clear(sstatus, 1UL << 18);
    } else {
        if (unlikely(syscall_id != 214)) {
            logkf("Syscall(%d) cannot implemented.\n", syscall_id);
        }
        regs->a0 = SYSCALL_FAULT_(ENOSYS);
    }
}
