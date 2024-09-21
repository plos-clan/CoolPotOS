#include "../include/rtc.h"
#include "../include/io.h"
#include "../include/printf.h"

uint8_t rtc_get_year() {
    outb(0x70, 0x09);
    return inb(0x71);
}

uint8_t rtc_get_month() {
    outb(0x70, 0x08);
    return inb(0x71);
}

uint8_t rtc_get_day() {
    outb(0x70, 0x07);
    return inb(0x71);
}

uint8_t rtc_get_weekday() {
    outb(0x70, 0x06);
    return inb(0x71);
}

uint8_t rtc_get_hour() {
    outb(0x70, 0x04);
    return inb(0x71);
}

uint8_t rtc_get_minute() {
    outb(0x70, 0x02);
    return inb(0x71);
}

uint8_t rtc_get_second() {
    outb(0x70, 0x00);
    return inb(0x71);
}

void rtc_print_time() {
    printf("20%x/%x.%x %x:%x:%x\n",
            rtc_get_year(), rtc_get_month(), rtc_get_day(),
            rtc_get_hour(), rtc_get_minute(), rtc_get_second());
}

void rtc_init() {
    klogf(true,"RTC Timer:");
    rtc_print_time();
}