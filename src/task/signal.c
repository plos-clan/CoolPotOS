#include "task/signal.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "task/signal_arch.h"
#include "task/task.h"
#include "term/klog.h"

static signal_internal_t signal_internal_decisions[MAXSIG] = { 0 };

bool signal_sig_in_range(int sig) {
    return sig >= MINSIG && sig <= MAXSIG;
}

bool signal_sig_maskable(int sig) {
    return sig >= MINSIG && sig < (int)(sizeof(sigset_t) * 8);
}

sigset_t signal_sigbit(int sig) {
    return (sigset_t)1ULL << (uint64_t)sig;
}

sigset_t sigset_user_to_kernel(sigset_t user_mask) {
    sigset_t kernel_mask = user_mask << 1;
    if (signal_sig_maskable(SIGKILL))
        kernel_mask &= ~signal_sigbit(SIGKILL);
    if (signal_sig_maskable(SIGSTOP))
        kernel_mask &= ~signal_sigbit(SIGSTOP);
    return kernel_mask;
}

sigset_t sigset_kernel_to_user(sigset_t kernel_mask) {
    return kernel_mask >> 1;
}

static void signal_fill_kernel_siginfo(siginfo_t *info, int sig, int code) {
    if (!info)
        return;

    memset(info, 0, sizeof(*info));
    info->si_signo = sig;
    info->si_errno = 0;
    info->si_code  = code;
    tcb_t sender   = get_current_task();
    if (sender) {
        info->_sifields._kill.si_pid = sender->process ? sender->process->pid : 0;
        info->_sifields._kill.si_uid = sender->process ? sender->process->uid : 0;
    }
}

static inline void signal_clear_pending_info(tcb_t task, int sig) {
    if (!task || !signal_sig_maskable(sig))
        return;

    task->pending_siginfo_mask &= ~signal_sigbit(sig);
    memset(&task->pending_siginfo[sig], 0, sizeof(task->pending_siginfo[sig]));
}

bool signals_pending_quick(const tcb_t task) {
    const sigset_t pending_list   = task->signal;
    const sigset_t unblocked_list = pending_list & ~task->blocked;
    for (int i = MINSIG; i <= MAXSIG; i++) {
        if (!(unblocked_list & SIGMASK(i))) {
            continue;
        }
        const sigaction_t *action       = &task->actions[i - 1];
        const sighandler_t user_handler = action->sa_handler;
        if (user_handler == SIG_IGN) {
            continue;
        }
        if (user_handler == SIG_DFL && signal_internal_decisions[i - 1] == SIGNAL_INTERNAL_IGN) {
            continue;
        }

        return true;
    }
    return false;
}

void signal_init() {
    signal_internal_decisions[SIGABRT - 1] = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGALRM - 1] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGBUS - 1]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGCHLD - 1] = SIGNAL_INTERNAL_IGN;
    // signal_internal_decisions[SIGCLD] = SIGNAL_INTERNAL_IGN;
    signal_internal_decisions[SIGCONT - 1] = SIGNAL_INTERNAL_CONT;
    // signal_internal_decisions[SIGEMT] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGFPE - 1]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGHUP - 1]  = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGILL - 1]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGINT - 1]  = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGIO - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGIOT - 1]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGKILL - 1] = SIGNAL_INTERNAL_TERM;
    // signal_internal_decisions[SIGLOST] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPIPE - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPOLL - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPROF - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPWR - 1]    = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGQUIT - 1]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGSEGV - 1]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGSTKFLT - 1] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGSTOP - 1]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGTSTP - 1]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGSYS - 1]    = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGTERM - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGTRAP - 1]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGTTIN - 1]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGTTOU - 1]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGUNUSED - 1] = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGURG - 1]    = SIGNAL_INTERNAL_IGN;
    signal_internal_decisions[SIGUSR1 - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGUSR2 - 1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGVTALRM - 1] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGXCPU - 1]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGXFSZ - 1]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGWINCH - 1]  = SIGNAL_INTERNAL_IGN;
}

int send_signal_to_process_info(const pcb_t process, const int sig, const siginfo_t *info) {
    if (process == NULL || !signal_sig_in_range(sig)) {
        return -EINVAL;
    }
    if (process->status == T_DEATH || process->status == T_ZOMBIE) {
        return -ESRCH;
    }

    tcb_t target = NULL;
    cow_foreach(process->child_threads, target) {
        break;
    }
    if (target == NULL) {
        return -ESRCH;
    }

    if (signal_sig_maskable(sig)) {
        const sigset_t bit = signal_sigbit(sig);
        if ((target->signal & bit) == 0) {
            if (info) {
                target->pending_siginfo[sig]          = *info;
                target->pending_siginfo[sig].si_signo = sig;
            } else {
                signal_fill_kernel_siginfo(&target->pending_siginfo[sig], sig, SI_KERNEL);
            }
            target->pending_siginfo_mask |= bit;
        }
    }

    target->signal |= SIGMASK(sig);

    if (target->status == T_WAIT) {
        target->status = T_RUNNING;
    }

    return 0;
}

int send_signal_to_process(const pcb_t process, const int sig) {
    return send_signal_to_process_info(process, sig, NULL);
}

int send_signal_to_pgroup_info(const pid_t pgid, const int sig, const siginfo_t *info) {
    if (!signal_sig_in_range(sig)) {
        return -EINVAL;
    }

    int sent      = 0;
    pcb_t process = NULL;
    cow_foreach(get_process_list(), process) {
        if (process->pgid == pgid) {
            if (send_signal_to_process_info(process, sig, info) == 0) {
                sent++;
            }
        }
    }
    return sent > 0 ? 0 : -ESRCH;
}

int send_signal_to_pgroup(const pid_t pgid, const int sig) {
    return send_signal_to_pgroup_info(pgid, sig, NULL);
}

void do_signal(struct syscall_regs *regs) {
    const tcb_t task = get_current_task();
    if (task == NULL) {
        return;
    }

    for (int sig = MINSIG; sig <= MAXSIG; sig++) {
        if (!(task->signal & SIGMASK(sig))) {
            continue;
        }
        if (task->blocked & SIGMASK(sig)) {
            continue;
        }

        sigaction_t *action        = &task->actions[sig - 1];
        const sighandler_t handler = action->sa_handler;

        if (sig == SIGKILL) {
            task->signal &= ~SIGMASK(sig);
            signal_clear_pending_info(task, sig);
            kill_proc(task->process, sig, true);
            return;
        }

        if (handler == SIG_IGN) {
            task->signal &= ~SIGMASK(sig);
            signal_clear_pending_info(task, sig);
            continue;
        }

        if (handler == SIG_DFL) {
            signal_internal_t decision = signal_internal_decisions[sig - 1];
            switch (decision) {
            case SIGNAL_INTERNAL_TERM:
            case SIGNAL_INTERNAL_CORE:
                task->signal &= ~SIGMASK(sig);
                signal_clear_pending_info(task, sig);
                kill_proc(task->process, sig, true);
                return;
            case SIGNAL_INTERNAL_IGN:
                task->signal &= ~SIGMASK(sig);
                signal_clear_pending_info(task, sig);
                continue;
            case SIGNAL_INTERNAL_STOP:
                task->signal &= ~SIGMASK(sig);
                signal_clear_pending_info(task, sig);
                // TODO: implement process stop
                continue;
            case SIGNAL_INTERNAL_CONT:
                task->signal &= ~SIGMASK(sig);
                signal_clear_pending_info(task, sig);
            }
            continue;
        }

        task->signal &= ~SIGMASK(sig);
        signal_clear_pending_info(task, sig);
        arch_signal_setup(task, sig, action, regs);
        return;
    }
}
