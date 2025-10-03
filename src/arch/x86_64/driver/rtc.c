#include "heap.h"
#include "io.h"
#include "sprintf.h"
#include "timer.h"

static int month[25] = {0, // 这里占位，没有 0 月，从 1 月开始
                        0,
                        (31),
                        (31 + 29),
                        (31 + 29 + 31),
                        (31 + 29 + 31 + 30),
                        (31 + 29 + 31 + 30 + 31),
                        (31 + 29 + 31 + 30 + 31 + 30),
                        (31 + 29 + 31 + 30 + 31 + 30 + 31),
                        (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
                        (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
                        (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
                        (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0};

#define bcd2hex(n) ((n >> 4) * 10) + (n & 0xf)

uint8_t read_cmos(uint8_t p) {
    uint8_t data;
    io_out8(CMOS_INDEX, p);
    data = io_in8(CMOS_DATA);
    io_out8(CMOS_INDEX, 0x80);
    return data;
}

uint32_t get_hour() {
    return bcd2hex(read_cmos(CMOS_CUR_HOUR));
}

uint32_t get_min() {
    return bcd2hex(read_cmos(CMOS_CUR_MIN));
}

uint32_t get_sec() {
    return bcd2hex(read_cmos(CMOS_CUR_SEC));
}

uint32_t get_day_of_month() {
    return bcd2hex(read_cmos(CMOS_MON_DAY));
}

uint32_t get_day_of_week() {
    return bcd2hex(read_cmos(CMOS_WEEK_DAY));
}

uint32_t get_mon() {
    return bcd2hex(read_cmos(CMOS_CUR_MON));
}

uint32_t get_year() {
    return (bcd2hex(read_cmos(CMOS_CUR_CEN)) * 100) + bcd2hex(read_cmos(CMOS_CUR_YEAR)) + 1980;
}

int is_leap_year(int year) {
    if (year % 4 != 0) return 0;
    if (year % 400 == 0) return 1;
    return year % 100 != 0;
}

int64_t mktime() {
    int64_t res;
    int     year;
    if (get_year() >= 70)
        year = get_year() - 70;
    else
        year = get_year() - 70 + 100;
    res  = YEAR * year;
    res += DAY * ((year + 1) / 4);
    res += month[get_mon()] * DAY;
    if (get_mon() > 2 && ((year + 2) % 4)) res -= DAY;
    res += DAY * (get_day_of_month() - 1);
    res += HOUR * get_hour();
    res += MINUTE * get_min();
    res += get_sec();

    return res;
}

char *get_date_time() {
    char *s    = (char *)malloc(40);
    int   year = get_year(), month = get_mon(), day = get_day_of_month();
    int   hour = get_hour(), min = get_min(), sec = get_sec();
    int   day_of_months[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap_year(year)) day_of_months[2]++;
#ifdef NEED_UTC_8
    hour += 8;
    if (hour >= 24) hour -= 24, day++;
    if (day > day_of_months[month]) day = 1, month++;
    if (month > 12) month = 1, year++;
#endif
    sprintf(s, "%d/%d/%d %d:%d:%d", year, month, day, hour, min, sec);
    return s;
}
