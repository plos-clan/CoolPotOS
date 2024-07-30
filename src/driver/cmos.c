#include "../include/cmos.h"
#include "../include/memory.h"
#include "../include/io.h"
#include "../include/graphics.h"
#include "../include/common.h"

static void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx) {
    *eax = op;
    *ecx = 0;
    asm volatile("cpuid"
            : "=a" (*eax),                //输出参数
    "=b" (*ebx),
    "=c" (*ecx),
    "=d" (*edx)
            : "0" (*eax), "2" (*ecx)    //输入参数
            : "memory");
}

static void get_vendor_name(cpu_t *c) {
    int cpuid_level;
    static char x86_vendor_id[16] = {0};
    cpuid(0x00000000, (unsigned int *) &cpuid_level,
          (unsigned int *) &x86_vendor_id[0],
          (unsigned int *) &x86_vendor_id[8],
          (unsigned int *) &x86_vendor_id[4]);
    c->vendor = x86_vendor_id;
}

static void get_model_name(cpu_t *c) {
    unsigned int *v = (unsigned int *) c->model_name;
    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
    c->model_name[48] = 0;
}

static void get_cpu_address_sizes(cpu_t *c) {
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

    c->virt_bits = (eax >> 8) & 0xff;
    c->phys_bits = eax & 0xff;
}

void print_cpu_id() {
    cpu_t *c = (cpu_t *) kmalloc(sizeof(cpu_t));
    get_vendor_name(c);
    get_model_name(c);
    get_cpu_address_sizes(c);
    printf("CPU Vendor:            %s\n", c->vendor);
    printf("CPU Name:              %s\n", c->model_name);
    printf("CPU Cache:             %d\n",c->phys_bits);
    printf("CPU Virtual Address:   0x%x\n",c->virt_bits);
    kfree(c);
}

uint8_t read_cmos(uint8_t p) {
    uint8_t data;
    outb(CMOS_INDEX, p);
    data = inb(CMOS_DATA);
    outb(CMOS_INDEX, 0x80);
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

char *get_date_time() {
    char *s = (char *) kmalloc(40);
    int year = get_year(), month = get_mon(), day = get_day_of_month();
    int hour = get_hour(), min = get_min(), sec = get_sec();
    int day_of_months[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
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