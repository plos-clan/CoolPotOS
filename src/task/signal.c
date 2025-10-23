#include "task/task.h"
#include "task/signal.h"
#include "errno.h"
signal_internal_t signal_internal_decisions[MAXSIG] = {0};

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
