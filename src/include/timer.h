#pragma once

#define RTC_PNP_ID "PNP0B00"

#define SECONDS_PER_MINUTE 60LL
#define SECONDS_PER_HOUR   (60LL * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY    (24LL * SECONDS_PER_HOUR)
#define EPOCH_YEAR         1970
#define DAYS_PER_YEAR      365

#include "types.h"

typedef uint64_t clock_t;

struct timespec {
    uint64_t tv_sec;
    uint64_t tv_nsec;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

typedef struct int_timer_internal {
    uint64_t at;
    uint64_t reset;
} int_timer_internal_t;

void arch_send_scheduler();
size_t sched_clock();
uint64_t nano_time();
int64_t mktime_universal();
void rtc_setup();

void ms_to_timeval(uint64_t ms, struct timeval *tv);

static inline clock_t clock() {
    return sched_clock();
}
