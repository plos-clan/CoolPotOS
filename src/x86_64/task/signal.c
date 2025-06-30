#include "signal.h"
#include "klog.h"
#include "pcb.h"

signal_internal_t signal_internal_decisions[MAXSIG] = {0};

void check_pending_signals(pcb_t proc, tcb_t thread) {}

void register_signal(pcb_t task, int sig, void (*handler)(int)) {}

bool signals_pending_quick(tcb_t task) {
    sigset_t pending_list   = task->signal;
    sigset_t unblocked_list = pending_list & (~task->blocked);
    for (int i = MINSIG; i <= MAXSIG; i++) {
        if (!(unblocked_list & SIGMASK(i))) continue;
        sigaction_t *action       = &task->actions[i];
        sighandler_t user_handler = action->sa_handler;
        if (user_handler == SIG_IGN) continue;
        if (user_handler == SIG_DFL && signal_internal_decisions[i] == SIGNAL_INTERNAL_IGN)
            continue;

        return true;
    }
    return false;
}

int syscall_ssetmask(int how, sigset_t *nset, sigset_t *oset) {
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

int syscall_sig_action(int sig, sigaction_t *action, sigaction_t *oldaction) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL) { return -EINVAL; }

    sigaction_t *ptr = &get_current_task()->actions[sig];
    if (oldaction) { *oldaction = *ptr; }

    if (action) { *ptr = *action; }

    if (ptr->sa_flags & SIG_NOMASK) {
        ptr->sa_mask = 0;
    } else {
        ptr->sa_mask |= SIGMASK(sig);
    }

    return 0;
}

void signal_init() {
    signal_internal_decisions[SIGABRT] = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGALRM] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGBUS]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGCHLD] = SIGNAL_INTERNAL_IGN;
    // signal_internal_decisions[SIGCLD] = SIGNAL_INTERNAL_IGN;
    signal_internal_decisions[SIGCONT] = SIGNAL_INTERNAL_CONT;
    // signal_internal_decisions[SIGEMT] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGFPE]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGHUP]  = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGILL]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGINT]  = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGIO]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGIOT]  = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGKILL] = SIGNAL_INTERNAL_TERM;
    // signal_internal_decisions[SIGLOST] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPIPE]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPOLL]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPROF]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGPWR]    = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGQUIT]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGSEGV]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGSTKFLT] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGSTOP]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGTSTP]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGSYS]    = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGTERM]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGTRAP]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGTTIN]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGTTOU]   = SIGNAL_INTERNAL_STOP;
    signal_internal_decisions[SIGUNUSED] = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGURG]    = SIGNAL_INTERNAL_IGN;
    signal_internal_decisions[SIGUSR1]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGUSR2]   = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGVTALRM] = SIGNAL_INTERNAL_TERM;
    signal_internal_decisions[SIGXCPU]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGXFSZ]   = SIGNAL_INTERNAL_CORE;
    signal_internal_decisions[SIGWINCH]  = SIGNAL_INTERNAL_IGN;
}
