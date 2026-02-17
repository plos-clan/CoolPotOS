#include "lib/acpica/acpi.h"
#include "term/klog.h"
#include "timer.h"

#ifdef __x86_64__
#    include "io.h"
#endif

uint64_t        rtc_irq          = 0;
static uint64_t rtc_io_port_cmd  = 0; // 0x70
static uint64_t rtc_io_port_data = 0; // 0x71

static const int days_in_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static inline int is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

#ifdef __x86_64__

#    define CMOS_ADDR 0x70
#    define CMOS_DATA 0x71
#    define RTC_SECONDS 0x00
#    define RTC_MINUTES 0x02
#    define RTC_HOURS 0x04
#    define RTC_DAY 0x07
#    define RTC_MONTH 0x08
#    define RTC_YEAR 0x09
#    define RTC_STATUS_A 0x0A
#    define RTC_STATUS_B 0x0B
#    define RTC_CENTURY 0x32

static uint8_t cmos_read(uint8_t reg) {
    io_out8(CMOS_ADDR, reg);
    return io_in8(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

static uint8_t read_rtc(uint8_t reg) {
    while (cmos_read(RTC_STATUS_A) & 0x80)
        ;
    uint8_t val = cmos_read(reg);
    if (!(cmos_read(RTC_STATUS_B) & 0x04)) {
        val = bcd_to_bin(val);
    }
    return val;
}

uint32_t get_full_year() {
    uint8_t year    = read_rtc(RTC_YEAR);
    uint8_t century = read_rtc(RTC_CENTURY);
    if (century == 0)
        century = 20;
    return (uint32_t)century * 100 + year;
}

uint32_t get_mon() {
    return read_rtc(RTC_MONTH);
}

uint32_t get_day_of_month() {
    return read_rtc(RTC_DAY);
}

uint32_t get_hour() {
    while (cmos_read(RTC_STATUS_A) & 0x80)
        ;
    uint8_t hour     = cmos_read(RTC_HOURS);
    uint8_t status_b = cmos_read(RTC_STATUS_B);
    int     is_pm    = hour & 0x80;
    if (!(status_b & 0x04)) {
        hour = bcd_to_bin(hour & 0x7F);
    } else {
        hour = hour & 0x7F;
    }
    if (!(status_b & 0x02) && is_pm) {
        hour = (hour + 12) % 24;
    }
    return hour;
}

uint32_t get_min() {
    return read_rtc(RTC_MINUTES);
}

uint32_t get_sec() {
    return read_rtc(RTC_SECONDS);
}

#else

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

#endif

int64_t mktime_universal() {
    int     current_year  = (int)get_full_year();
    int     current_month = (int)get_mon();
    int     current_day   = (int)get_day_of_month();
    int64_t total_days    = 0;
    for (int year = EPOCH_YEAR; year < current_year; year++) {
        total_days += DAYS_PER_YEAR;
        if (is_leap_year(year)) {
            total_days++;
        }
    }
    for (int month = 1; month < current_month; month++) {
        int days = days_in_month[month];
        if (month == 2 && is_leap_year(current_year)) {
            days++;
        }
        total_days += days;
    }
    total_days += (current_day - 1);
    int64_t total_seconds = total_days * SECONDS_PER_DAY;
    total_seconds += (int64_t)get_hour() * SECONDS_PER_HOUR;
    total_seconds += (int64_t)get_min() * SECONDS_PER_MINUTE;
    total_seconds += get_sec();
    return total_seconds;
}

void rtc_device_init() {
}

void rtc_setup() {
    // TODO
}
