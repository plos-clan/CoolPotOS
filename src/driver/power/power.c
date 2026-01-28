#include "driver/power/power.h"
#include "krlibc.h"
#include "lib/acpica/acpi.h"
#include "task/scheduler.h"
#include "term/klog.h"

void power_restart() {
    ACPI_STATUS status;
    kinfo("ACPI: Attempting hardware reset...");
    status = AcpiReset();
    if (ACPI_FAILURE(status)) { kerror("ACPI: Reset failed: %s", AcpiFormatException(status)); }
    for (;;)
        arch_wait_for_interrupt();
}

void power_off() {
    ACPI_STATUS status;
    kinfo("ACPI: Preparing to enter S5 state...");
    status = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
    if (ACPI_FAILURE(status)) {
        kerror("ACPI: Failed to prepare for S5: %s", AcpiFormatException(status));
        return;
    }
    arch_close_interrupt();
    disable_scheduler();
    status = AcpiEnterSleepState(ACPI_STATE_S5);
    kerror("ACPI: Failed to enter S5: %s", AcpiFormatException(status));
    for (;;)
        arch_wait_for_interrupt();
}

static UINT32 AcpiFixedEventPowerButtonHandler(void *Context) {

    // signal_shutdown_event();
    power_off();

    return ACPI_INTERRUPT_HANDLED;
}

void power_button_init() {
    ACPI_STATUS status;
    AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
    status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, AcpiFixedEventPowerButtonHandler,
                                          NULL);
    if (ACPI_FAILURE(status)) {
        kerror("Failed to install fixed power button handler: %s", AcpiFormatException(status));
        return;
    }
    status = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    if (ACPI_FAILURE(status)) {
        kerror("Failed to enable fixed power button.");
    } else {
        kinfo("ACPI: Fixed Power Button initialized.");
    }
}
