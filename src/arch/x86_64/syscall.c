#include "syscall.h"
#include "errno.h"
#include "fsgsbase.h"
#include "io.h"
#include "krlibc.h"
#include "nr.h"
#include "ptrace.h"
#include "task/signal.h"
#include "task/task.h"
#include "term/klog.h"

syscall_(arch_prctl, uint64_t code, uint64_t addr); // prsys_x64.c
syscall_(sched_getaffinity, pid_t pid, size_t cpusetsize, unsigned long *mask); // prsys.c
syscall_(lgetxattr, const char *path, const char *name, void *value, size_t size); // fssys.c
syscall_(llistxattr, const char *path, char *list, size_t size); // fssys.c

__attribute__((naked)) void asm_syscall_handle() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "cld\n\t"
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov [rax+0x08], rsp\n\t"
                     "cmp QWORD PTR [rax+0x18], 0\n\t"
                     "je normal\n\t"
                     "signal:\n\t" // 信号处理
                     "mov rsp, [rax+0x10]\n\t"
                     "jmp next\n\t"
                     "normal:\n\t" // 默认处理
                     "mov rsp, [rax+0x0] \n\t"
                     "jmp next\n\t"
                     "next:\n\t"
                     "sub rsp, 0x38\n\t"
                     "mov rax, cr2\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "push rbp\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push r8\n\t"
                     "push r9\n\t"
                     "push r10\n\t"
                     "push r11\n\t"
                     "push r12\n\t"
                     "push r13\n\t"
                     "push r14\n\t"
                     "push r15\n\t"
                     "mov rdi, rsp\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsi, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "call syscall_handler\n\t"
                     "pop r15\n\t"
                     "pop r14\n\t"
                     "pop r13\n\t"
                     "pop r12\n\t"
                     "pop r11\n\t"
                     "pop r10\n\t"
                     "pop r9\n\t"
                     "pop r8\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop rbp\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "add rsp, 0x38\n\t"
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsp, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "sysretq\n\t" ::
                         : "memory");
}

void arch_enable_syscall() {
    uint64_t efer;
    efer = rdmsr(MSR_EFER);
    efer |= 1;
    uint64_t star = ((uint64_t)((0x18 | 0x3) - 8) << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_EFER, efer);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)asm_syscall_handle);
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}

syscall_(brk_dump) {
    return SYSCALL_FAULT_(ENOSYS);
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [12]                    = (syscall_t)syscall_brk_dump, // brk no impl
    [SYSCALL_EXIT]          = (syscall_t)syscall_exit,
    [SYSCALL_OPEN]          = (syscall_t)syscall_open,
    [SYSCALL_CLOSE]         = (syscall_t)syscall_close,
    [SYSCALL_ARCH_PRCTL]    = (syscall_t)syscall_arch_prctl,
    [SYSCALL_SETID_ADDR]    = (syscall_t)syscall_set_tid_address,
    [SYSCALL_WRITE]         = (syscall_t)syscall_write,
    [SYSCALL_READ]          = (syscall_t)syscall_read,
    [SYSCALL_WRITEV]        = (syscall_t)syscall_writev,
    [SYSCALL_READV]         = (syscall_t)syscall_readv,
    [SYSCALL_GETPID]        = (syscall_t)syscall_getpid,
    [SYSCALL_EXIT_GROUP]    = (syscall_t)syscall_exit_group,
    [SYSCALL_GETUID]        = (syscall_t)syscall_getuid,
    [SYSCALL_GETGID]        = (syscall_t)syscall_getgid,
    [SYSCALL_STAT]          = (syscall_t)syscall_stat,
    [SYSCALL_IOCTL]         = (syscall_t)syscall_ioctl,
    [SYSCALL_DUP]           = (syscall_t)syscall_dup,
    [SYSCALL_DUP2]          = (syscall_t)syscall_dup2,
    [SYSCALL_DUP3]          = (syscall_t)syscall_dup3,
    [SYSCALL_GETCWD]        = (syscall_t)syscall_getcwd,
    [SYSCALL_CHDIR]         = (syscall_t)syscall_chdir,
    [SYSCALL_YIELD]         = (syscall_t)syscall_yield,
    [SYSCALL_UNAME]         = (syscall_t)syscall_uname,
    [SYSCALL_MMAP]          = (syscall_t)syscall_mmap,
    [SYSCALL_MREMAP]        = (syscall_t)syscall_mremap,
    [SYSCALL_MUNMAP]        = (syscall_t)syscall_munmap,
    [SYSCALL_MADVISE]       = (syscall_t)syscall_madvise,
    [SYSCALL_FCNTL]         = (syscall_t)syscall_fcntl,
    [SYSCALL_MOUNT]         = (syscall_t)syscall_mount,
    [SYSCALL_FSTAT]         = (syscall_t)syscall_fstat,
    [SYSCALL_C_GETTIME]     = (syscall_t)syscall_clock_gettime,
    [SYSCALL_C_GETRES]      = (syscall_t)syscall_clock_getres,
    [SYSCALL_GETPGID]       = (syscall_t)syscall_getpgid,
    [SYSCALL_GETSID]        = (syscall_t)syscall_getsid,
    [SYSCALL_SETSID]        = (syscall_t)syscall_setsid,
    [SYSCALL_SETPGID]       = (syscall_t)syscall_setpgid,
    [SYSCALL_MPROTECT]      = (syscall_t)syscall_mprotect,
    [SYSCALL_GETPPID]       = (syscall_t)syscall_getppid,
    [SYSCALL_POLL]          = (syscall_t)syscall_poll,
    [SYSCALL_RT_SIGMASK]    = (syscall_t)syscall_ssetmask,
    [SYSCALL_SIGPENDING]    = (syscall_t)syscall_sigpending,
    [SYSCALL_SIGTIMEDWAIT]  = (syscall_t)syscall_sigtimedwait,
    [SYSCALL_SIGQUEUEINFO]  = (syscall_t)syscall_sigqueueinfo,
    [SYSCALL_SIGSUSPEND]    = (syscall_t)syscall_sigsuspend,
    [SYSCALL_SIGALTSTACK]   = (syscall_t)syscall_sigaltstack,
    [SYSCALL_MKNOD]         = (syscall_t)syscall_mknod,
    [SYSCALL_SIGACTION]     = (syscall_t)syscall_sig_action,
    [SYSCALL_SIGRET]        = (syscall_t)syscall_sigret,
    [SYSCALL_MINCORE]       = (syscall_t)syscall_mincore,
    [SYSCALL_UMOUNT2]       = (syscall_t)syscall_umount2,
    [SYSCALL_GETEGID]       = (syscall_t)syscall_getegid,
    [SYSCALL_GETEUID]       = (syscall_t)syscall_geteuid,
    [SYSCALL_WAITPID]       = (syscall_t)syscall_waitpid,
    [SYSCALL_FUTEX]         = (syscall_t)syscall_futex,
    [SYSCALL_LSEEK]         = (syscall_t)syscall_lseek,
    [SYSCALL_PREAD]         = (syscall_t)syscall_pread,
    [SYSCALL_PWRITE]        = (syscall_t)syscall_pwrite,
    [SYSCALL_CP_F_RANGE]    = (syscall_t)syscall_copy_file_range,
    [SYSCALL_GETGROUPS]     = (syscall_t)syscall_getgroups,
    [SYSCALL_RENAME]        = (syscall_t)syscall_rename,
    [SYSCALL_SYMLINK]       = (syscall_t)syscall_symlink,
    [SYSCALL_LINK]          = (syscall_t)syscall_link,
    [SYSCALL_NANO_SLEEP]    = (syscall_t)syscall_nano_sleep,
    [SYSCALL_GET_TID]       = (syscall_t)syscall_get_tid,
    [SYSCALL_LGETXATTR]     = (syscall_t)syscall_lgetxattr,
    [SYSCALL_LLISTXATTR]    = (syscall_t)syscall_llistxattr,
    [SYSCALL_SELECT]        = (syscall_t)syscall_select,
    [SYSCALL_PSELECT6]      = (syscall_t)syscall_pselect6,
    [SYSCALL_GETDENTS64]    = (syscall_t)syscall_getdents,
    [SYSCALL_NEWFSTATAT]    = (syscall_t)syscall_newfstatat,
    [SYSCALL_STATX]         = (syscall_t)syscall_statx,
    [SYSCALL_PIPE2]         = (syscall_t)syscall_pipe2,
    [SYSCALL_PIPE]          = (syscall_t)syscall_pipe,
    [SYSCALL_UNLINK]        = (syscall_t)syscall_unlink,
    [SYSCALL_UNLINKAT]      = (syscall_t)syscall_unlinkat,
    [SYSCALL_RMDIR]         = (syscall_t)syscall_rmdir,
    [SYSCALL_ACCESS]        = (syscall_t)syscall_access,
    [SYSCALL_MKDIR]         = (syscall_t)syscall_mkdir,
    [SYSCALL_MKDIRAT]       = (syscall_t)syscall_mkdirat,
    [SYSCALL_PRCTL]         = (syscall_t)syscall_prctl,
    [SYSCALL_FORK]          = (syscall_t)syscall_fork,
    [SYSCALL_EXECVE]        = (syscall_t)syscall_execve,
    [SYSCALL_VFORK]         = (syscall_t)syscall_vfork,
    [SYSCALL_CLONE]         = (syscall_t)syscall_clone,
    [SYSCALL_LSTAT]         = (syscall_t)syscall_lstat,
    [SYSCALL_SYSINFO]       = (syscall_t)syscall_sysinfo,
    [SYSCALL_READLINK]      = (syscall_t)syscall_readlink,
    [SYSCALL_CHMOD]         = (syscall_t)syscall_chmod,
    [SYSCALL_FCHMOD]        = (syscall_t)syscall_fchmod,
    [SYSCALL_FCHMODAT]      = (syscall_t)syscall_fchmodat,
    [SYSCALL_SENDFILE]      = (syscall_t)syscall_sendfile,
    [SYSCALL_OPENAT]        = (syscall_t)syscall_openat,
    [SYSCALL_FACCESSAT]     = (syscall_t)syscall_faccessat,
    [SYSCALL_FACCESSAT2]    = (syscall_t)syscall_faccessat2,
    [SYSCALL_STATFS]        = (syscall_t)syscall_statfs,
    [SYSCALL_GETRLIMIT]     = (syscall_t)syscall_get_rlimit,
    [SYSCALL_GETRUSAGE]     = (syscall_t)syscall_getrusage,
    [SYSCALL_TIMES]         = (syscall_t)syscall_times,
    [SYSCALL_PRLIMIT64]     = (syscall_t)syscall_prlimit64,
    [SYSCALL_GETRESGID]     = (syscall_t)syscall_getresgid,
    [SYSCALL_GETRESUID]     = (syscall_t)syscall_getresuid,
    [SYSCALL_SYSLOG]        = (syscall_t)syscall_sys_log,
    [SYSCALL_CHROOT]        = (syscall_t)syscall_chroot,
    [SYSCALL_SETITIMER]     = (syscall_t)syscall_setitimer,
    [SYSCALL_KILL]          = (syscall_t)syscall_kill,
    [SYSCALL_G_AFFINITY]    = (syscall_t)syscall_sched_getaffinity,
    [SYSCALL_CAPGET]        = (syscall_t)syscall_capget,
    [SYSCALL_CAPSET]        = (syscall_t)syscall_capset,
    [SYSCALL_CHOWN]         = (syscall_t)syscall_chown,
    [SYSCALL_FCHOWN]        = (syscall_t)syscall_fchown,
    [SYSCALL_LCHOWN]        = (syscall_t)syscall_lchown,
    [SYSCALL_FCHOWNAT]      = (syscall_t)syscall_fchownat,
    [SYSCALL_UMASK]         = (syscall_t)syscall_umask,
    [SYSCALL_UTIMENSAT]     = (syscall_t)syscall_utimensat,
    [SYSCALL_FUTIMESAT]     = (syscall_t)syscall_futimensat,
    [SYSCALL_SYNC]          = (syscall_t)syscall_sync,
    [SYSCALL_REBOOT]        = (syscall_t)syscall_reboot,
    [SYSCALL_GETRANDOM]     = (syscall_t)syscall_getrandom,
    [SYSCALL_SOCKET]        = (syscall_t)syscall_socket,
    [SYSCALL_SOCKETPAIR]    = (syscall_t)syscall_socketpair,
    [SYSCALL_BIND]          = (syscall_t)syscall_bind,
    [SYSCALL_LISTEN]        = (syscall_t)syscall_listen,
    [SYSCALL_ACCEPT]        = (syscall_t)syscall_accept,
    [SYSCALL_CONNECT]       = (syscall_t)syscall_connect,
    [SYSCALL_SENDTO]        = (syscall_t)syscall_sendto,
    [SYSCALL_RECVFROM]      = (syscall_t)syscall_recvfrom,
    [SYSCALL_SENDMSG]       = (syscall_t)syscall_sendmsg,
    [SYSCALL_RECVMSG]       = (syscall_t)syscall_recvmsg,
    [SYSCALL_SHUTDOWN]      = (syscall_t)syscall_shutdown,
    [SYSCALL_SETSOCKOPT]    = (syscall_t)syscall_setsockopt,
    [SYSCALL_GETSOCKOPT]    = (syscall_t)syscall_getsockopt,
    [SYSCALL_GETSOCKNAME]   = (syscall_t)syscall_getsockname,
    [SYSCALL_GETPEERNAME]   = (syscall_t)syscall_getpeername,
    [SYSCALL_EPOLL_CREATE1] = (syscall_t)syscall_epoll_create1,
    [SYSCALL_EPOLL_CTL]     = (syscall_t)syscall_epoll_ctl,
    [SYSCALL_EPOLL_WAIT]    = (syscall_t)syscall_epoll_wait,
    [SYSCALL_EPOLL_PWAIT]   = (syscall_t)syscall_epoll_pwait,
    [SYSCALL_EVENTFD2]      = (syscall_t)syscall_eventfd2,
};

USED void syscall_handler(struct syscall_regs *regs, uint64_t user_regs) { // syscall 指令处理
    regs->rip    = regs->rcx;
    regs->rflags = regs->r11;
    regs->cs     = (0x20 | 0x3);
    regs->ss     = (0x18 | 0x3);
    regs->ds     = (0x18 | 0x3);
    regs->es     = (0x18 | 0x3);
    regs->rsp    = user_regs;

    tcb_t thread = get_current_task();
    write_fsbase((uint64_t)thread);

    uint64_t syscall_id = regs->rax & 0xFFFFFFFF;

    if (likely(syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL)) {
        arch_open_interrupt();
        regs->rax = (syscall_handlers[syscall_id])(
            regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9, regs
        );
        arch_close_interrupt();
    } else {
        logkf("Syscall(%d) cannot implemented.\n", syscall_id);
        regs->rax = -ENOSYS;
    }

    if (syscall_id != SYSCALL_SIGRET) {
        do_signal(regs);
    }

    // If sigsuspend set saved_sigmask and do_signal didn't invoke a handler, restore it now
    if (thread->has_saved_sigmask) {
        thread->blocked           = thread->saved_sigmask;
        thread->has_saved_sigmask = false;
    }

    write_fsbase(thread->context.fs_base);
}
