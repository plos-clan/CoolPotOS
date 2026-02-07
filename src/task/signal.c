#include "task/signal.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "task/signal_arch.h"
#include "task/task.h"

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

extern cow_arraylist *process_list;

int send_signal_to_process(pcb_t process, int sig) {
    if (process == NULL || sig < MINSIG || sig > MAXSIG) return -EINVAL;
    if (process->status == T_DEATH || process->status == T_ZOMBIE) return -ESRCH;

    tcb_t target = NULL;
    cow_foreach(process->child_threads, target) {
        break;
    }
    if (target == NULL) return -ESRCH;

    target->signal |= SIGMASK(sig);

    if (target->status == T_WAIT) { target->status = T_RUNNING; }

    return 0;
}

int send_signal_to_pgroup(pid_t pgid, int sig) {
    if (sig < MINSIG || sig > MAXSIG) return -EINVAL;

    int   sent    = 0;
    pcb_t process = NULL;
    cow_foreach(process_list, process) {
        if (process->pgid == pgid) {
            if (send_signal_to_process(process, sig) == 0) sent++;
        }
    }
    return sent > 0 ? 0 : -ESRCH;
}

void do_signal(struct syscall_regs *regs) {
    tcb_t task = get_current_task();
    if (task == NULL) return;

    for (int sig = MINSIG; sig <= MAXSIG; sig++) {
        if (!(task->signal & SIGMASK(sig))) continue;
        if (task->blocked & SIGMASK(sig)) continue;

        sigaction_t *action  = &task->actions[sig];
        sighandler_t handler = action->sa_handler;

        if (sig == SIGKILL) {
            task->signal &= ~SIGMASK(sig);
            kill_proc(task->process, sig, true);
            return;
        }

        if (handler == SIG_IGN) {
            task->signal &= ~SIGMASK(sig);
            continue;
        }

        if (handler == SIG_DFL) {
            signal_internal_t decision  = signal_internal_decisions[sig];
            task->signal               &= ~SIGMASK(sig);
            switch (decision) {
            case SIGNAL_INTERNAL_TERM:
            case SIGNAL_INTERNAL_CORE: kill_proc(task->process, sig, true); return;
            case SIGNAL_INTERNAL_IGN: continue;
            case SIGNAL_INTERNAL_STOP:
                // TODO: implement process stop
                continue;
            case SIGNAL_INTERNAL_CONT:
                // TODO: implement process continue
                continue;
            }
            continue;
        }

        arch_signal_setup(task, sig, action, regs);
        return;
    }
}
