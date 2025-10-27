#pragma once

#define RTC_PNP_ID "PNP0B00"

#define SECONDS_PER_MINUTE  60LL
#define SECONDS_PER_HOUR    (60LL * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY     (24LL * SECONDS_PER_HOUR)
#define EPOCH_YEAR          1970
#define DAYS_PER_YEAR       365

#include "types.h"

void arch_send_scheduler();
size_t sched_clock();
uint64_t nano_time();
int64_t mktime_universal();
void rtc_setup();
