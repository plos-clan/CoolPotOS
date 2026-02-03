#include "errno.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/memstat.h"
#include "syscall.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer.h"

#include <driver/power/power.h>

extern cow_arraylist *process_list;

syscall_(uname, struct utsname *utsname) {
    if (unlikely(utsname == NULL)) return SYSCALL_FAULT_(EINVAL);
    char sysname[] = "CoolPotOS";
    char machine[] = "x86_64";
    char version[] = "0.0.1";
    memcpy(utsname->sysname, sysname, sizeof(sysname));
    memcpy(utsname->nodename, "localhost", 50);
    memcpy(utsname->release, KERNEL_NAME, sizeof(KERNEL_NAME));
    memcpy(utsname->version, version, sizeof(version));
    memcpy(utsname->machine, machine, sizeof(machine));
    return EOK;
}

syscall_(clock_gettime, uint64_t arg0, struct timespec *ts) {
    switch (arg0) {
    case 1:
    case 6:
    case 4: {
        if (ts != NULL) {
            uint64_t nano = nano_time();
            ts->tv_sec    = nano / 1000000000ULL;
            ts->tv_nsec   = nano % 1000000000ULL;
        }
        return EOK;
    }
    case 0: {
        uint64_t timestamp = mktime_universal();
        if (ts != NULL) {
            ts->tv_sec  = timestamp;
            ts->tv_nsec = 0;
        }
        return EOK;
    }
    case 7: {
        if (ts != NULL) {
            uint64_t nano = sched_clock();
            ts->tv_sec    = nano / 1000000000ULL;
            ts->tv_nsec   = nano % 1000000000ULL;
        }
        return EOK;
    }
    default: logkf("clock not supported(%d)\n", arg0); return SYSCALL_FAULT_(EINVAL);
    }
}

syscall_(clock_getres) {
    if (arg2 == 0) return SYSCALL_FAULT_(EINVAL);
    ((struct timespec *)arg2)->tv_nsec = 1000000;
    return EOK;
}

syscall_(getgroups, int count, int *gid_list) {
    if (count > 0) {
        gid_list[0] = 0;
        return 1;
    }
    return 0;
}

syscall_(nano_sleep, void *time_handle) {
    struct timespec k_req;
    if (unlikely(time_handle == NULL)) return SYSCALL_FAULT_(EINVAL);
    memcpy(&k_req, time_handle, sizeof(k_req));
    if (unlikely(k_req.tv_nsec >= 1000000000L)) return SYSCALL_FAULT_(EINVAL);
    uint64_t nsec = k_req.tv_sec * 1000000000 + k_req.tv_nsec;
    scheduler_nano_sleep(nsec);
    return EOK;
}

syscall_(sysinfo, struct sysinfo *info) {
    if (check_user_overflow((uint64_t)info, sizeof(struct sysinfo))) return SYSCALL_FAULT_(EFAULT);
    memset(info, 0, sizeof(struct sysinfo));
    info->freeram  = get_available_memory();
    info->totalram = get_all_memory();
    info->mem_unit = 1;
    info->procs    = process_list->size;
    return EOK;
}

syscall_(sys_log, int type, const char *buf, size_t len) {
    switch (type) {
    case 3:
        if (len <= 0) { return SYSCALL_FAULT_(EINVAL); }
        if (check_user_overflow((uint64_t)buf, len)) { return SYSCALL_FAULT_(EFAULT); }
        return kmesg_read((uint8_t *)buf, len);
    case 4: return kmsg_read_all((uint8_t *)buf, len);
    case 5: return kmsg_length();
    case 9: kmsg_empty(); return EOK;
    default: return SYSCALL_FAULT_(EINVAL);
    }
}

syscall_(setitimer, int which, struct itimerval *value, struct itimerval *old) {
    if (which != 0) return (size_t)-ENOSYS;

    pcb_t process = get_current_task()->process;

    uint64_t rt_at    = process->itimer_real.at;
    uint64_t rt_reset = process->itimer_real.reset;

    uint64_t now = nano_time() / 1000000;

    if (old) {
        uint64_t remaining = rt_at > now ? rt_at - now : 0;
        ms_to_timeval(remaining, &old->it_value);
        ms_to_timeval(rt_reset, &old->it_interval);
    }

    if (value) {
        uint64_t targValue = value->it_value.tv_sec * 1000 + value->it_value.tv_usec / 1000;
        uint64_t targInterval =
            value->it_interval.tv_sec * 1000 + value->it_interval.tv_usec / 1000;

        process->itimer_real.at    = targValue ? (now + targValue) : 0ULL;
        process->itimer_real.reset = targInterval;
    }

    return 0;
}

syscall_(reboot, int magic1, int magic2, uint32_t cmd, void *arg) {
    if (magic1 != LINUX_REBOOT_MAGIC1 || magic2 != LINUX_REBOOT_MAGIC2) return (uint64_t)-EINVAL;
    switch (cmd) {
    case LINUX_REBOOT_CMD_CAD_OFF: return EOK;
    case LINUX_REBOOT_CMD_CAD_ON: return EOK;
    case LINUX_REBOOT_CMD_RESTART:
    case LINUX_REBOOT_CMD_RESTART2: power_restart(); return EOK;
    case LINUX_REBOOT_CMD_POWER_OFF: power_off(); return EOK;
    default: return (uint64_t)-EINVAL;
    }
}
