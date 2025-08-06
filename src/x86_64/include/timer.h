#pragma once

#define NEED_UTC_8

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

#define CMOS_CUR_SEC  0x0
#define CMOS_CUR_MIN  0x2
#define CMOS_CUR_HOUR 0x4
#define CMOS_WEEK_DAY 0x6
#define CMOS_MON_DAY  0x7
#define CMOS_CUR_MON  0x8
#define CMOS_CUR_YEAR 0x9
#define CMOS_CUR_CEN  0x32

#define MINUTE 60
#define HOUR   (60 * MINUTE)
#define DAY    (24 * HOUR)
#define YEAR   (365 * DAY)

#include "ctype.h"
#include "isr.h"

typedef struct timer {
    uint64_t old_time;
} timer_t;

//void timer_handle(void); __attribute__((naked));
void     nsleep(uint64_t nano);
uint64_t nano_time();
uint64_t elapsed();

uint32_t get_hour();
uint32_t get_min();
uint32_t get_sec();
uint32_t get_day_of_month();
uint32_t get_day_of_week();
uint32_t get_mon();
uint32_t get_year();
int      is_leap_year(int year);
char    *get_date_time();
int64_t  mktime();
timer_t *alloc_timer();
uint64_t get_time(timer_t *timer1);
