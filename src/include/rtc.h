#ifndef CRASHPOWEROS_RTC_H
#define CRASHPOWEROS_RTC_H

#include <stdint.h>

uint8_t rtc_get_year();
uint8_t rtc_get_month();
uint8_t rtc_get_day();
uint8_t rtc_get_weekday();
uint8_t rtc_get_hour();
uint8_t rtc_get_minute();
uint8_t rtc_get_second();
void rtc_print_time();
void rtc_init();

#endif
