#include "timer.h"
#include "syscall.h"
#include "task/task.h"
#include "errno.h"

void ms_to_timeval(const uint64_t ms, struct timeval *tv) {
    tv->tv_sec  = ms / 1000;
    tv->tv_usec = ms % 1000 * 1000; // 转换为微秒保持结构体定义
}

syscall_(timer_create, clockid_t clockid, struct sigevent *sevp, timer_t *timerid) {
    const pcb_t process = get_current_task()->process;

    kernel_timer_t *kt = NULL;
    uint64_t i;
    for (i = 0; i < MAX_TIMERS_NUM; i++) {
        if (process->timers[i] == NULL) {
            kt                 = malloc(sizeof(kernel_timer_t));
            process->timers[i] = kt;
            break;
        }
    }

    if (!kt) {
        return SYSCALL_FAULT_(ENOMEM);
    }

    memset(kt, 0, sizeof(kernel_timer_t));

    kt->clock_type   = clockid;
    kt->sigev_notify = SIGEV_SIGNAL;

    if (sevp) {
        struct sigevent ksev;
        memcpy(&ksev, sevp, sizeof(struct sigevent));

        kt->sigev_signo  = ksev.sigev_signo;
        kt->sigev_value  = ksev.sigev_value;
        kt->sigev_notify = ksev.sigev_notify;
    }

    *timerid = (timer_t)i;

    return EOK;
}

syscall_(
    timer_settime, timer_t timerid, const struct itimerval *new_value, struct itimerval *old_value
) {
    uint64_t idx = (uint64_t)timerid;
    if (idx >= MAX_TIMERS_NUM) {
        return SYSCALL_FAULT_(EINVAL);
    }
    const pcb_t process = get_current_task()->process;

    kernel_timer_t *kt = process->timers[idx];

    struct itimerval kts;
    memcpy(&kts, new_value, sizeof(*new_value));

    const uint64_t interval =
        new_value->it_interval.tv_sec * 1000 + new_value->it_interval.tv_usec / 1000;
    const uint64_t expires = new_value->it_value.tv_sec * 1000 + new_value->it_value.tv_usec / 1000;

    const uint64_t now = nano_time() / 1000000;

    if (old_value) {
        struct itimerval old;
        old.it_interval.tv_sec  = kt->interval / 1000;
        old.it_interval.tv_usec = kt->interval % 1000 * 1000000;
        old.it_value.tv_sec     = (kt->expires - now) / 1000;
        old.it_value.tv_usec    = (kt->expires - now) % 1000 * 1000000;
        memcpy(old_value, &old, sizeof(old));
    }

    kt->interval = interval;
    kt->expires  = now + expires;

    task_refresh_tick_work_state(process);

    return EOK;
}
