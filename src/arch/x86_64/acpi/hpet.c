#include "hpet.h"
#include "krlibc.h"
#include "lib/acpica/acpi.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"
#include "timer.h"

HpetInfo       *hpet_addr;
static uint64_t hpetPeriod   = 0;
static uint64_t fms_per_tick = 0;

uint64_t nano_time() {
    if (hpet_addr == NULL) return 0;
    uint64_t mcv = hpet_addr->mainCounterValue;
    return mcv * hpetPeriod;
}

uint64_t elapsed() {
    uint64_t mcv = mmio_read64((void *)hpet_addr + 0xf0);
    return (uint64_t)((mcv * fms_per_tick) / 1000000U);
}

void nsleep(uint64_t nano) {
    uint64_t targetTime = nano_time();
    uint64_t after      = 0;
    while (true) {
        uint64_t n = nano_time();
        if (n < targetTime) {
            after      += 0xffffffff - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) { return; }
    }
}

void hpet_init() {
    ACPI_TABLE_HPET *hpet_table = NULL;
    ACPI_STATUS      status     = AcpiGetTable(ACPI_SIG_HPET, 1, (ACPI_TABLE_HEADER **)&hpet_table);
    if (ACPI_FAILURE(status)) {
        kerror("ACPI: HPET table not found.");
        return;
    }
    ACPI_GENERIC_ADDRESS *gas = &hpet_table->Address;
    if (gas->SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY) {
        kerror("ACPI: Error! HPET is not MMIO.");
        return;
    }
    hpet_addr = (HpetInfo *)phys_to_virt(gas->Address);
    page_map_range(get_kernel_pagedir(), (uint64_t)hpet_addr, gas->Address, PAGE_SIZE,
                   KERNEL_PTE_FLAGS);
    uint32_t counterClockPeriod                         = hpet_addr->generalCapabilities >> 32;
    hpetPeriod                                          = counterClockPeriod / 1000000;
    hpet_addr->generalConfiguration                    |= 1;
    *(volatile uint64_t *)((uint64_t)hpet_addr + 0xf0)  = 0;
    fms_per_tick                                        = hpet_addr->mainCounterValue;
    kinfo("Setup acpi hpet table (nano_time: %#ld).", nano_time());
}
