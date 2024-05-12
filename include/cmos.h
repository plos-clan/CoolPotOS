#ifndef CRASHPOWEROS_CMOS_H
#define CRASHPOWEROS_CMOS_H

#include <stdint.h>

#define NEED_UTC_8

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

#define CMOS_CUR_SEC 0x0
#define CMOS_CUR_MIN 0x2
#define CMOS_CUR_HOUR 0x4
#define CMOS_WEEK_DAY 0x6
#define CMOS_MON_DAY 0x7
#define CMOS_CUR_MON 0x8
#define CMOS_CUR_YEAR 0x9
#define CMOS_CUR_CEN 0x32

#define bcd2hex(n) ((n >> 4) * 10) + (n & 0xf)

typedef struct {
    char* vendor;
    char model_name[64];
    unsigned int virt_bits;
    unsigned int phys_bits;
}cpu_t;

uint8_t read_cmos(uint8_t p);
char *get_date_time();
void print_cpu_id();

uint32_t get_hour();
uint32_t get_min();
uint32_t get_sec();
uint32_t get_day_of_month();
uint32_t get_day_of_week();
uint32_t get_mon();
uint32_t get_year();
int is_leap_year(int year);

#endif //CRASHPOWEROS_CMOS_H
