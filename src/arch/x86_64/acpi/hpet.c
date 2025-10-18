#include "hpet.h"
#include "driver/uacpi/acpi.h"
#include "driver/uacpi/tables.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"
#include "timer.h"

HpetInfo       *hpet_addr;
static uint64_t hpetPeriod = 0;

uint64_t nano_time() {
    if (hpet_addr == NULL) return 0;
    uint64_t mcv = hpet_addr->mainCounterValue;
    return mcv * hpetPeriod;
}

void hpet_init() {
    struct uacpi_table hpet_table;
    uacpi_status       status = uacpi_table_find_by_signature("HPET", &hpet_table);

    if (status == UACPI_STATUS_OK) {
        struct acpi_hpet *hpet = hpet_table.ptr;

        hpet_addr = (HpetInfo *)phys_to_virt(hpet->address.address);
        page_map_range(get_kernel_pagedir(), (uint64_t)hpet_addr, hpet->address.address, PAGE_SIZE,
                       KERNEL_PTE_FLAGS);
        uint32_t counterClockPeriod                         = hpet_addr->generalCapabilities >> 32;
        hpetPeriod                                          = counterClockPeriod / 1000000;
        hpet_addr->generalConfiguration                    |= 1;
        *(volatile uint64_t *)((uint64_t)hpet_addr + 0xf0)  = 0;
        kinfo("Setup acpi hpet table (nano_time: %#ld).", nano_time());
    }
}
