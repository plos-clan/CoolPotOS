#include "driver/acpi.h"
#include "driver/uacpi/event.h"
#include "driver/uacpi/sleep.h"
#include "driver/uacpi/uacpi.h"
#include "driver/uacpi/utilities.h"
#include "krlibc.h"
#include "term/klog.h"

// #include "boot.h"
// #include "lib/neoacpi/neoacpi.h"

void acpi_init() {
    //
    // struct neo_acpi_handle *handle = neo_acpi_initialize(boot_get_acpi_rsdp());
    // if (!handle) { kerror("cannot initialize acpi"); }
    //
    // while (true)
    //     arch_wait_for_interrupt();

    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        printk("uacpi_initialize error: %s\n", uacpi_status_to_string(ret));
        while (true)
            arch_wait_for_interrupt();
    }
}

void acpi_namespace_setup() {
    uacpi_status ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        kerror("uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
        return;
    }
    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        kerror("uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
    }
    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        kerror("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
    }
}
