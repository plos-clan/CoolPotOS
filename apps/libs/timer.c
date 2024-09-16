#include "../include/time.h"
#include "../include/stdlib.h"
#include "../include/rand.h"

#define JAN_1970 0x83aa7e80
#define COMMON_YEAR_SEC 31536000
#define LEAP_YEAR_SEC 31622400
#define DAY_SEC 86400

struct tm *tm_ = NULL;

static int table1[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int table2[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void UnTimeStamp(uint32_t timestamp, uint32_t *year, uint32_t *month,
                 uint32_t *day, uint32_t *hour, uint32_t *min, uint32_t *sec,
                 uint32_t *yday, uint32_t *mday, uint32_t *wday) {
    timestamp += 28800;
    uint32_t y = 1970;
    for (;; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
            timestamp -= LEAP_YEAR_SEC;
            if (timestamp <= COMMON_YEAR_SEC) {
                break;
            }
        } else {
            timestamp -= COMMON_YEAR_SEC;
            if (timestamp <= COMMON_YEAR_SEC ||
                ((((y + 1) % 4 == 0 && (y + 1) % 100 != 0) || (y + 1) % 400 == 0) &&
                 timestamp <= LEAP_YEAR_SEC)) {
                break;
            }
        }
    }
    *year = y + 1;
    uint32_t month0 = 1;
    if ((*year % 4 == 0 && *year % 100 != 0) || *year % 400 == 0) {
        for (; timestamp > table2[month0 - 1] * DAY_SEC; month0++) {
            timestamp -= table2[month0 - 1] * DAY_SEC;
        }
    } else {
        for (; timestamp > table1[month0 - 1] * DAY_SEC; month0++) {
            timestamp -= table1[month0 - 1] * DAY_SEC;
        }
    }
    *month = month0;
    *day = timestamp / DAY_SEC + 1;
    timestamp = timestamp % DAY_SEC;
    *hour = timestamp / 3600;
    timestamp = timestamp % 3600;
    *min = timestamp / 60;
    *sec = timestamp % 60;

    // 计算一年中的第几天
    int days = 0;
    if ((*year % 4 == 0 && *year % 100 != 0) || *year % 400 == 0) {
        for (int i = 0; i < month0 - 1; i++) {
            days += table2[i];
        }
    } else {
        for (int i = 0; i < month0 - 1; i++) {
            days += table1[i];
        }
    }
    days += *day;
    *yday = days;

    // 计算一个月中的第几天
    *mday = *day;
    int totalDays = *yday; // 一年中的第几天，从0开始计数
    int startYear = 1970;
    int startWeekday = 1; // 1970年1月1日是星期一
    for (int year1 = startYear; year1 <= *year; year1++) {
        if ((year1 % 4 == 0 && year1 % 100 != 0) || year1 % 400 == 0) {
            totalDays += 366; // 闰年有366天
        } else {
            totalDays += 365; // 平年有365天
        }
    }
    int weekday = (startWeekday + totalDays) % 7;
    *wday = weekday;
    return;
}

struct tm *localtime(time_t *t1) {
    time_t t = *t1;
    if (!tm_) {
        tm_ = malloc(sizeof(struct tm));
    }
    UnTimeStamp(t, &(tm_->tm_year), &(tm_->tm_mon), &(tm_->tm_mday),
                &(tm_->tm_hour), &(tm_->tm_min), &(tm_->tm_sec), &(tm_->tm_yday),
                &(tm_->tm_mday),&(tm_->tm_wday));
    tm_->tm_year -= 1970;
    tm_->tm_mon--;
    return tm_;
}

double difftime(time_t t1, time_t t0) { return t1 - t0; }

void clock_gettime(int *sec1, int *usec1) {
    int b = clock() * 10;
    *sec1 = b / 1000;
    *usec1 = (b % 1000) * 1000;
}

time_t time(time_t timer) { return rand(); }

time_t mktime(struct tm *tm) {
    // 判断年份是否在有效范围内（1970年以后）
    if (tm->tm_year < 70) {
        return (time_t)-1;
    }

    // 将月份调整为从 0 开始（0 表示一月，1 表示二月，以此类推）
    tm->tm_mon -= 1;

    // 计算1970年到tm->tm_year年的天数
    int year, leapYearCount;
    int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    leapYearCount = (tm->tm_year - 69) / 4; // 计算闰年的个数
    year = 70 + tm->tm_year;

    int totalDays = 0;
    for (int i = 70; i < year; i++) {
        if (i % 4 == 0 && (i % 100 != 0 || i % 400 == 0)) {
            totalDays += 366; // 闰年有 366 天
        } else {
            totalDays += 365; // 平年有 365 天
        }
    }

    // 计算tm->tm_mon前的月份的天数
    for (int i = 0; i < tm->tm_mon; i++) {
        totalDays += monthDays[i];
    }

    // 若是闰年且过了2月，则总天数加一
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) && tm->tm_mon > 1) {
        totalDays += 1;
    }

    // 加上日期差值
    totalDays += tm->tm_mday - 1;

    // 计算总的秒数
    time_t seconds = totalDays * 24 * 60 * 60 + tm->tm_hour * 60 * 60 +
                     tm->tm_min * 60 + tm->tm_sec;

    return seconds;
}

