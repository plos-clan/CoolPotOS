#include "lib/acpica/acpi.h"
#include "term/klog.h"
#include "timer.h"

uint64_t        rtc_irq          = 0;
static uint64_t rtc_io_port_cmd  = 0; // 0x70
static uint64_t rtc_io_port_data = 0; // 0x71

static const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static inline int is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

uint32_t get_full_year() {
    return 0;
}
uint32_t get_mon() {
    return 0;
}
uint32_t get_day_of_month() {
    return 0;
}
uint32_t get_hour() {
    return 0;
}
uint32_t get_min() {
    return 0;
}
uint32_t get_sec() {
    return 0;
}

int64_t mktime_universal() {
    int     current_year  = (int)get_full_year();
    int     current_month = (int)get_mon();
    int     current_day   = (int)get_day_of_month();
    int64_t total_days    = 0;
    for (int year = EPOCH_YEAR; year < current_year; year++) {
        total_days += DAYS_PER_YEAR;
        if (is_leap_year(year)) { total_days++; }
    }
    for (int month = 1; month < current_month; month++) {
        int days = days_in_month[month];
        if (month == 2 && is_leap_year(current_year)) { days++; }
        total_days += days;
    }
    total_days            += (current_day - 1);
    int64_t total_seconds  = total_days * SECONDS_PER_DAY;
    total_seconds         += (int64_t)get_hour() * SECONDS_PER_HOUR;
    total_seconds         += (int64_t)get_min() * SECONDS_PER_MINUTE;
    total_seconds         += get_sec();
    return total_seconds;
}

void rtc_device_init() {}

void rtc_setup() {
    //TODO
}
