#include "driver/uacpi/resources.h"
#include "driver/uacpi/utilities.h"
#include "term/klog.h"
#include "timer.h"

uint64_t        rtc_irq          = 0;
static uint64_t rtc_io_port_cmd  = 0; // 0x70
static uint64_t rtc_io_port_data = 0; // 0x71

static const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static inline int is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

uint32_t get_full_year(){
    return 0;
}
uint32_t get_mon(){
    return 0;
}
uint32_t get_day_of_month(){
    return 0;
}
uint32_t get_hour(){
    return 0;
}
uint32_t get_min(){
    return 0;
}
uint32_t get_sec(){
    return 0;
}

int64_t mktime_universal() {
    int current_year  = (int)get_full_year();
    int current_month = (int)get_mon();
    int current_day   = (int)get_day_of_month();
    int64_t total_days = 0;
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

void rtc_device_init() {}

static uacpi_iteration_decision iteration_decision(void *user, uacpi_resource *resource) {
    if (resource == NULL) return UACPI_ITERATION_DECISION_BREAK;
    kinfo("RTC Resource Found: Type %d", resource->type);
    if (resource->type == UACPI_RESOURCE_TYPE_IO) {
        const uacpi_resource_io *io_res = &resource->io;
        if (io_res->minimum == 0x70 && io_res->length == 0x02) {
            rtc_io_port_cmd  = io_res->minimum;
            rtc_io_port_data = io_res->minimum + 1;
            kinfo("Found General IO ports: 0x%x and 0x%x", rtc_io_port_cmd, rtc_io_port_data);
            return UACPI_ITERATION_DECISION_NEXT_PEER;
        }
    }
    if (resource->type == UACPI_RESOURCE_TYPE_FIXED_IO) {
        if (resource->fixed_io.address == 0x70 && resource->fixed_io.length == 0x02) {
            rtc_io_port_cmd  = resource->fixed_io.address;
            rtc_io_port_data = resource->fixed_io.address + 1;
            kinfo("found rtc IO ports: 0x%llx and 0x%llx", rtc_io_port_cmd, rtc_io_port_data);
        }
    }
    if (resource->type == UACPI_RESOURCE_TYPE_IRQ) {
        for (uacpi_u32 i = 0; i < resource->irq.num_irqs; i++) {
            uint64_t current_irq = resource->irq.irqs[i];
            if (current_irq == 8) {
                rtc_irq = current_irq;
                kinfo("found rtc device IRQ: %llu", rtc_irq);
                break;
            }
        }
    }
    if (resource->type == UACPI_RESOURCE_TYPE_END_TAG) return UACPI_ITERATION_DECISION_BREAK;
    return UACPI_ITERATION_DECISION_NEXT_PEER;
}

static uacpi_iteration_decision match_rtc(void *user, uacpi_namespace_node *node, uacpi_u32 i) {
    uacpi_resources *kb_res;
    uacpi_status     ret = uacpi_get_current_resources(node, &kb_res);
    if (uacpi_unlikely_error(ret)) {
        kwarn("unable to retrieve PS2K resources: %s", uacpi_status_to_string(ret));
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }
    uacpi_for_each_resource(kb_res, iteration_decision, user);
    rtc_device_init();
    uacpi_free_resources(kb_res);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void rtc_setup() {
    uacpi_find_devices(RTC_PNP_ID, match_rtc, NULL);
}
