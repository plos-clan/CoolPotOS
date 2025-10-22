#include "errno.h"
#include "krlibc.h"
#include "syscall.h"
#include "term/klog.h"
#include "timer.h"

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
