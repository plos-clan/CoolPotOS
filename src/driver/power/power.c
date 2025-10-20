#include "driver/power/power.h"
#include "driver/uacpi/event.h"
#include "driver/uacpi/sleep.h"
#include "driver/uacpi/uacpi.h"
#include "term/klog.h"
#include "krlibc.h"

void power_restart(){
    uacpi_reboot();
}

void power_off() {
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        kerror("failed to prepare for sleep: %s", uacpi_status_to_string(ret));
        return;
    }
    arch_close_interrupt();
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        kerror("failed to enter sleep: %s", uacpi_status_to_string(ret));
    }
}

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    kwarn("The kernel is shutting down..");
    uacpi_kernel_sleep(100);
    power_off();
    return UACPI_INTERRUPT_HANDLED;
}

void power_button_init() {
    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
        handle_power_button, UACPI_NULL
    );
    if (uacpi_unlikely_error(ret)) {
        kerror("failed to install power button event callback: %s", uacpi_status_to_string(ret));
    }
}
