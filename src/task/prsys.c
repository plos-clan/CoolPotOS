#include "errno.h"
#include "mem/frame.h"
#include "krlibc.h"
#include "syscall.h"
#include "task/futex.h"
#include "task/scheduler.h"
#include "task/signal.h"
#include "task/signal_arch.h"
#include "task/task.h"
#include "term/klog.h"

syscall_(exit, int exit_code) {
    tcb_t exit_thread = get_current_task();
    logkf("sys_exit: Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    pcb_t process = exit_thread->process;
    if (process->child_threads->size <= 1) {
        kill_proc(process, exit_code, true);
    } else
        kill_thread(exit_thread);
    arch_open_interrupt();
    while (true)
        arch_wait_for_interrupt();
    return EOK;
}

syscall_(set_tid_address, int *tidptr) {
    tcb_t thread          = get_current_task();
    thread->tid_address   = (uint64_t)tidptr;
    thread->tid_directory = get_current_directory();
    return thread->tid;
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
    kill_proc(exit_process, exit_code, true);
    arch_open_interrupt();
    while (true)
        arch_wait_for_interrupt();
}

syscall_(getuid) {
    return get_current_task()->process->uid;
}

syscall_(getgid) {
    return get_current_task()->process->rgid;
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

syscall_(getsid, pid_t pid) {
    pcb_t process = pid == 0 ? get_current_task()->process : found_pcb(pid);
    if (process == NULL || process->status == T_DEATH) { return SYSCALL_FAULT_(ESRCH); }
    return process->pgid;
}

syscall_(getppid) {
    pcb_t process = get_current_task()->process;
    return process->parent->pid;
}

syscall_(ssetmask, int how, sigset_t *nset, sigset_t *oset) {
    if (oset) *oset = get_current_task()->blocked;
    if (nset) {
        uint64_t safe  = *nset;
        safe          &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));
        switch (how) {
        case SIG_BLOCK: get_current_task()->blocked |= safe; break;
        case SIG_UNBLOCK: get_current_task()->blocked &= ~(safe); break;
        case SIG_SETMASK: get_current_task()->blocked = safe; break;
        default: return -EINVAL;
        }
    }
    return EOK;
}

syscall_(sigaltstack, altstack_t *old_stack, altstack_t *new_stack) {
    tcb_t thread = get_current_task();
    if (old_stack) { *old_stack = thread->alt_stack; }
    if (new_stack) { thread->alt_stack = *new_stack; }
    return EOK;
}

syscall_(sig_action, int sig, sigaction_t *action, sigaction_t *oldaction) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL) { return -EINVAL; }

    sigaction_t *ptr = &get_current_task()->actions[sig];
    if (oldaction) { *oldaction = *ptr; }

    if (action) { *ptr = *action; }

    if (ptr->sa_flags & SIG_NOMASK) {
        ptr->sa_mask = 0;
    } else {
        ptr->sa_mask |= SIGMASK(sig);
    }

    return EOK;
}

static int pick_pending_signal(sigset_t pending) {
    for (int i = MINSIG; i <= MAXSIG; i++) {
        if (pending & SIGMASK(i)) return i;
    }
    return 0;
}

syscall_(sigpending, sigset_t *set, size_t sigsetsize) {
    if (set == NULL) return SYSCALL_FAULT_(EINVAL);
    if (sigsetsize < sizeof(sigset_t)) return SYSCALL_FAULT_(EINVAL);
    *set = get_current_task()->signal;
    return EOK;
}

syscall_(sigtimedwait, const sigset_t *set, siginfo_t *info, const struct timespec *timeout,
         size_t sigsetsize) {
    if (set == NULL) return SYSCALL_FAULT_(EINVAL);
    if (sigsetsize < sizeof(sigset_t)) return SYSCALL_FAULT_(EINVAL);

    sigset_t mask = *set;
    if (mask == 0) return SYSCALL_FAULT_(EINVAL);

    uint64_t timeout_ns = 0;
    bool     has_timeout = false;
    if (timeout) {
        if (timeout->tv_nsec >= 1000000000ULL) return SYSCALL_FAULT_(EINVAL);
        timeout_ns  = timeout->tv_sec * 1000000000ULL + timeout->tv_nsec;
        has_timeout = true;
    }

    uint64_t start = nano_time();
    while (true) {
        tcb_t    thread  = get_current_task();
        sigset_t pending = thread->signal & mask;
        if (pending) {
            int signum = pick_pending_signal(pending);
            if (signum > 0) { thread->signal &= ~SIGMASK(signum); }
            if (info) {
                memset(info, 0, sizeof(*info));
                info->si_signo = signum;
            }
            return signum > 0 ? signum : SYSCALL_FAULT_(EAGAIN);
        }
        if (has_timeout) {
            uint64_t now = nano_time();
            if (now - start >= timeout_ns) return SYSCALL_FAULT_(EAGAIN);
        }
        arch_open_interrupt();
        arch_pause();
        arch_close_interrupt();
    }
}

syscall_(sigqueueinfo, pid_t pid, int sig, siginfo_t *info) {
    if (sig < MINSIG || sig > MAXSIG) return SYSCALL_FAULT_(EINVAL);
    pcb_t process = found_pcb(pid);
    if (process == NULL || process->status == T_DEATH) return SYSCALL_FAULT_(ESRCH);

    tcb_t target = NULL;
    cow_foreach(process->child_threads, target) {
        break;
    }
    if (target == NULL) return SYSCALL_FAULT_(ESRCH);

    target->signal |= SIGMASK(sig);
    (void)info;
    return EOK;
}

syscall_(sigsuspend, const sigset_t *mask, size_t sigsetsize) {
    if (mask == NULL) return SYSCALL_FAULT_(EINVAL);
    if (sigsetsize < sizeof(sigset_t)) return SYSCALL_FAULT_(EINVAL);
    sigset_t old = get_current_task()->blocked;

    get_current_task()->blocked = (uint64_t)*mask;
    while (!(get_current_task()->signal & ~get_current_task()->blocked)) {
        scheduler_yield();
    }
    get_current_task()->blocked = old;
    return SYSCALL_FAULT_(EINTR);
}

syscall_(signal, int sig, void *handler) {
    if (sig < 0 || sig >= MAX_SIGNALS) return SYSCALL_FAULT_(EINVAL);
    if (handler == NULL) return SYSCALL_FAULT_(EINVAL);
    logkf("Signal syscall: %p\n", handler);
    //TODO register_signal(get_current_task()->process, sig, handler);
    return EOK;
}

syscall_(sigret) {
    return arch_signal_sigreturn(regs);
}

syscall_(getegid) {
    //TODO EGID获取不支持
    return 0;
}

syscall_(geteuid) {
    return get_current_task()->process->uid;
}

syscall_(waitpid, pid_t pid, int *status, uint64_t options, struct rusage *rusage) {
    logkf("[fd-dbg] pid=%d waitpid(%d, options=0x%x)\n",
          get_current_task()->process->pid, pid, options);

    if (get_current_task()->process->child_process->size == 0) return SYSCALL_FAULT_(ECHILD);
    if (pid == -1) goto wait;
    pcb_t wait_p = found_pcb(pid);
    if (wait_p == NULL) return SYSCALL_FAULT_(ECHILD);
wait:;
    pid_t ret_pid = 0;
    int   status0 = waitpid(pid, &ret_pid, (options & WNOHANG) != 0);

    logkf("[fd-dbg] pid=%d waitpid() = %d (status=0x%x)\n",
          get_current_task()->process->pid, ret_pid, status0);

    if (ret_pid == 0) { return 0; }
    if (status) { *status = ((status0 & 0xFF) << 8); }
    if (rusage) { memset(rusage, 0, sizeof(struct rusage)); }
    return ret_pid;
}

syscall_(futex, int *uaddr, int op, int val, struct timespec *time, int timeout) {
    tcb_t thread = get_current_task();
re_futex: //TODO PRIVATE 标志暂时不支持
    switch (op) {
    case FUTEX_WAIT:
        if (uaddr == NULL) return SYSCALL_FAULT_(EINVAL);
        int observed = *uaddr;
        if (observed != val) { return SYSCALL_FAULT_(EAGAIN); }
        thread->status = T_FUTEX; // 挂起当前线程
        futex_add((void *)arch_virt_to_phys((uint64_t)uaddr), thread);
        scheduler_yield();
        return EOK;
    case FUTEX_WAKE:
        if (uaddr == NULL) return SYSCALL_FAULT_(EINVAL);
        futex_wake((void *)arch_virt_to_phys((uint64_t)uaddr), val);
        return EOK;
    default: return SYSCALL_FAULT_(EINVAL);
    }
}

syscall_(get_tid) {
    return get_current_task()->tid;
}

syscall_(prctl, int option) {
    switch (option) {
    case PR_SET_NAME:
        if (arg2 == 0) return -1;
        char *new_name = (char *)arg2;
        int   length   = strlen(new_name);
        if (length > 16) return -1;
        memcpy(get_current_task()->name, new_name, length);
        break;
    case PR_GET_NAME:
        if (arg2 == 0) return -1;
        char *proc_name = (char *)arg2;
        memcpy(proc_name, get_current_task()->name, 16);
        break;
    case PR_GET_DUMPABLE: return 0; //TODO CP_Kernel 不支持核心转储
    case PR_SET_DUMPABLE: return -1;
    default: return -1;
    }
    return EOK;
}

syscall_(get_rlimit, uint64_t resource, struct rlimit *lim) {
    if (!lim || check_user_overflow((uint64_t)lim, sizeof(struct rlimit))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    pcb_t process = get_current_task()->process;
    switch (resource) {
    case RLIMIT_STACK:
        *lim = (struct rlimit){
            .rlim_max = STACK_SIZE,
            .rlim_cur = STACK_SIZE,
        };
        break;
    case RLIMIT_NPROC:
        *lim = (struct rlimit){
            .rlim_cur = -1, // 不限制子进程数量
            .rlim_max = -1,
        };
        break;
    case RLIMIT_NOFILE:
        *lim = (struct rlimit){
            .rlim_cur = MAX_TASK_FD,
            .rlim_max = MAX_TASK_FD,
        };
        break;
    case RLIMIT_AS:
        *lim = (struct rlimit){
            .rlim_cur = process->vma_manager.vm_used,
            .rlim_max = process->vma_manager.vm_total,
        };
        break;
    case RLIMIT_CORE: *lim = (struct rlimit){0, 0}; break;

    default: return SYSCALL_FAULT_(EINVAL);
    }
    return EOK;
}

syscall_(prlimit64, uint64_t pid, int resource, const struct rlimit *new_rlim,
         struct rlimit *old_rlim) {
    if (new_rlim && check_user_overflow((uint64_t)new_rlim, sizeof(struct rlimit))) {
        return (uint64_t)-EFAULT;
    }
    if (old_rlim) {
        uint64_t ret = syscall_get_rlimit(resource, old_rlim, 0, 0, 0, 0, regs);
        if (ret != 0) return ret;
    }

    return EOK;
}

syscall_(getresgid, int *rgid, int *egid, int *sgid) {
    pcb_t process = get_current_task()->process;
    *rgid         = process->rgid;
    *egid         = process->egid;
    *sgid         = process->sgid;
    return EOK;
}

syscall_(getresuid, int *ruid, int *euid, int *suid) {
    pcb_t process = get_current_task()->process;
    *ruid         = process->ruid;
    *euid         = process->euid;
    *suid         = process->uid;
    return EOK;
}

syscall_(kill, int pid, int sig) {
    if (sig < 0 || sig > MAXSIG) return SYSCALL_FAULT_(EINVAL);

    // sig == 0: permission check only
    if (sig == 0) {
        if (pid > 0) {
            pcb_t process = found_pcb(pid);
            return process ? EOK : SYSCALL_FAULT_(ESRCH);
        }
        return EOK;
    }

    if (pid > 0) {
        // Send to specific process
        pcb_t process = found_pcb(pid);
        if (process == NULL) return SYSCALL_FAULT_(ESRCH);
        return send_signal_to_process(process, sig) == 0 ? EOK : SYSCALL_FAULT_(ESRCH);
    } else if (pid == 0) {
        // Send to caller's process group
        pcb_t self = get_current_task()->process;
        return send_signal_to_pgroup(self->pgid, sig) == 0 ? EOK : SYSCALL_FAULT_(ESRCH);
    } else if (pid == -1) {
        // Send to all processes (simplified: skip kernel process)
        extern cow_arraylist *process_list;
        extern pcb_t          kernel_process;
        pcb_t process = NULL;
        int   sent    = 0;
        cow_foreach(process_list, process) {
            if (process->pid == kernel_process->pid) continue;
            if (send_signal_to_process(process, sig) == 0) sent++;
        }
        return sent > 0 ? EOK : SYSCALL_FAULT_(ESRCH);
    } else {
        // pid < -1: send to process group |pid|
        return send_signal_to_pgroup(-pid, sig) == 0 ? EOK : SYSCALL_FAULT_(ESRCH);
    }
}

syscall_(times, struct tms *buf) {
    if (buf) { memset(buf, 0, sizeof(struct tms)); }
    return 0;
}

syscall_(getrusage, int who, struct rusage *usage) {
    if (usage) { memset(usage, 0, sizeof(struct rusage)); }
    return EOK;
}
