#include "driver/acpi.h"
#include "driver/uacpi/uacpi.h"
#include "driver/uacpi/utilities.h"
#include "term/klog.h"
#include "krlibc.h"
#include "limine.h"

LIMINE_REQUEST struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision  = 0
};

uintptr_t boot_get_acpi_rsdp() {
    return (uintptr_t)rsdp_request.response->address;
}

void acpi_init() {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        printk("uacpi_initialize error: %s", uacpi_status_to_string(ret));
        while (true) arch_wait_for_interrupt();
    }
}
