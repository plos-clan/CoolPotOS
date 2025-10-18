#include "apic.h"
#include "driver/uacpi/acpi.h"
#include "driver/uacpi/tables.h"
#include "timer.h"
#include "intctl.h"
#include "io.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"

bool     x2apic_mode = false;
uint64_t lapic_address;

uint64_t calibrated_timer_initial;

static struct ioapic_info found_ioapics[MAX_IOAPICS];
static struct iso_info    found_isos[MAX_ISO];

static size_t found_iso_count    = 0;
static size_t found_ioapic_count = 0;

void disable_pic() {
    io_out8(0x21, 0xff);
    io_out8(0xa1, 0xff);

    io_out8(0x20, 0x20);
    io_out8(0xa0, 0x20);

    io_out8(0x22, 0x70);
    io_out8(0x23, 0x01);
}

void lapic_write(uint32_t reg, uint32_t value) {
    if (x2apic_mode) {
        wrmsr(0x800 + (reg >> 4), value);
        return;
    }
    *(volatile uint32_t *)((uint64_t)lapic_address + reg) = value;
}

uint32_t lapic_read(uint32_t reg) {
    if (x2apic_mode) { return rdmsr(0x800 + (reg >> 4)); }
    return *(volatile uint32_t *)((uint64_t)lapic_address + reg);
}

uint64_t lapic_id() {
    uint32_t phy_id = lapic_read(LAPIC_REG_ID);
    return x2apic_mode ? phy_id : (phy_id >> 24);
}

static void ioapic_mmio_write(uintptr_t base, uint32_t reg, uint32_t value) {
    mmio_write32((void *)base, reg);
    mmio_write32((void *)(base + 0x10), value);
}

static uint32_t ioapic_mmio_read(uintptr_t base, uint32_t reg) {
    mmio_write32((void *)base, reg);
    return mmio_read32((void *)(base + 0x10));
}

uint32_t isa_irq_to_gsi(uint8_t isa_irq) {
    for (size_t i = 0; i < found_iso_count; i++) {
        if (found_isos[i].irq_source == isa_irq) { return found_isos[i].gsi; }
    }
    return isa_irq;
}

static struct ioapic_info *find_ioapic(uint32_t gsi) {
    for (size_t i = 0; i < found_ioapic_count; i++) {
        if (gsi >= found_ioapics[i].gsi_base &&
            gsi < found_ioapics[i].gsi_base + found_ioapics[i].irq_count) {
            return &found_ioapics[i];
        }
    }
    return NULL; // 没找到
}

void ioapic_add(uint8_t vector, uint32_t irq) {
    struct ioapic_info *io = find_ioapic(irq);
    if (!io) return;

    uint32_t irq0     = irq - irq;
    uint32_t ioredtbl = 0x10 + irq0 * 2;
    uint64_t redirect = vector | ((uint64_t)lapic_id() << 56);

    ioapic_mmio_write(io->mmio_base, ioredtbl, (uint32_t)redirect);
    ioapic_mmio_write(io->mmio_base, ioredtbl + 1, (uint32_t)(redirect >> 32));
}

void ioapic_enable(uint8_t vector) {
    struct ioapic_info *ioapic = find_ioapic(isa_irq_to_gsi(vector));
    if (!ioapic) {
        printk("Cannot found ioapic for vector %d\n", vector);
        return;
    }
    uint64_t index = 0x10 + ((isa_irq_to_gsi(vector) - ioapic->gsi_base) * 2);
    uint64_t value = (uint64_t)ioapic_mmio_read(ioapic->mmio_base, index + 1) << 32 |
                     (uint64_t)ioapic_mmio_read(ioapic->mmio_base, index);
    value &= (~0x10000UL);
    ioapic_mmio_write(ioapic->mmio_base, index, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_mmio_write(ioapic->mmio_base, index + 1, (uint32_t)(value >> 32));
}

void ioapic_disable(uint8_t vector) {
    struct ioapic_info *ioapic = find_ioapic(isa_irq_to_gsi(vector));
    if (!ioapic) {
        printk("Cannot found ioapic for vector %d\n", vector);
        return;
    }
    uint64_t index = 0x10 + ((isa_irq_to_gsi(vector) - ioapic->gsi_base) * 2);
    uint64_t value = (uint64_t)ioapic_mmio_read(ioapic->mmio_base, index + 1) << 32 |
                     (uint64_t)ioapic_mmio_read(ioapic->mmio_base, index);
    value |= 0x10000UL;
    ioapic_mmio_write(ioapic->mmio_base, index, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_mmio_write(ioapic->mmio_base, index + 1, (uint32_t)(value >> 32));
}

void send_eoi(int64_t irq) {
    lapic_write(0xb0, 0);
}

void lapic_timer_stop() {
    lapic_write(LAPIC_REG_TIMER_INITCNT, 0);
    lapic_write(LAPIC_REG_TIMER, (1 << 16));
}

static void apic_handle_ioapic(struct acpi_madt_ioapic *ioapic_madt) {
    struct ioapic_info *ioapic = &found_ioapics[found_ioapic_count];
    found_ioapic_count++;

    uint64_t mmio_phys = ioapic_madt->address;
    uint64_t mmio_virt = (uint64_t)phys_to_virt(mmio_phys);
    page_map_range(get_kernel_pagedir(), mmio_virt, mmio_phys, PAGE_SIZE, KERNEL_PTE_FLAGS);
    ioapic->mmio_base = mmio_virt;

    ioapic->gsi_base  = ioapic_madt->gsi_base;
    ioapic->irq_count = (ioapic_mmio_read(ioapic->mmio_base, 0x01) & 0x00FF0000) >> 16;

    printk("IOAPIC found: MMIO %p, GSI base %d, IRQs %d\n", (void *)ioapic->mmio_base,
           ioapic->gsi_base, ioapic->irq_count);

    ioapic->id = ioapic_madt->id;
}

static void apic_handle_override(struct acpi_madt_interrupt_source_override *override_madt) {
    struct iso_info *override = &found_isos[found_iso_count];
    found_iso_count++;
    override->irq_source = override_madt->source;
    override->gsi        = override_madt->gsi;
}

void local_apic_init() {
    uint64_t data  = rdmsr(0x1b);
    data          |= 1UL << 11;
    if (x2apic_mode) { data |= 1UL << 10; }
    wrmsr(0x1b, data);

    lapic_timer_stop();

    lapic_write(LAPIC_REG_SPURIOUS, 0xff | 1 << 8);
    lapic_write(LAPIC_REG_TIMER_DIV, 11);
    lapic_write(LAPIC_REG_TIMER, timer);

    uint64_t b = nano_time();
    lapic_write(LAPIC_REG_TIMER_INITCNT, ~((uint32_t)0));
    for (;;)
        if (nano_time() - b >= 100000000) break;
    uint64_t lapic_timer     = (~(uint32_t)0) - lapic_read(LAPIC_REG_TIMER_CURCNT);
    calibrated_timer_initial = (uint64_t)((uint64_t)(lapic_timer * 1000) / LAPIC_TIMER_SPEED);
    lapic_write(LAPIC_REG_TIMER, lapic_read(LAPIC_REG_TIMER) | 1 << 17);
    lapic_write(LAPIC_REG_TIMER_INITCNT, calibrated_timer_initial);

    kinfo("Calibrated LAPIC timer: %llu ticks per second.", calibrated_timer_initial);
    kinfo("Setup local %s.", x2apic_mode ? "x2APIC" : "APIC");
}

void apic_init() {
    struct uacpi_table madt_table;
    uacpi_status       status = uacpi_table_find_by_signature("APIC", &madt_table);
    if (status != UACPI_STATUS_OK) return;
    struct acpi_madt *madt = (struct acpi_madt *)madt_table.ptr;
    lapic_address = (uint64_t)phys_to_virt((uint64_t)madt->local_interrupt_controller_address);
    page_map_range(get_kernel_pagedir(), lapic_address, madt->local_interrupt_controller_address,
                   PAGE_SIZE, KERNEL_PTE_FLAGS);

    uint64_t current = 0;
    for (;;) {
        if (current + ((uint32_t)sizeof(struct acpi_madt) - 1) >= madt->hdr.length) { break; }
        struct acpi_entry_hdr *header =
            (struct acpi_entry_hdr *)((uint64_t)(&madt->entries) + current);
        if (header->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
            struct acpi_madt_ioapic *ioapic =
                (struct acpi_madt_ioapic *)((uint64_t)(&madt->entries) + current);
            apic_handle_ioapic(ioapic);
        } else if (header->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
            struct acpi_madt_interrupt_source_override *override =
                (struct acpi_madt_interrupt_source_override *)((uint64_t)(&madt->entries) +
                                                               current);
            apic_handle_override(override);
        }
        current += (uint64_t)header->length;
    }

    disable_pic();
    local_apic_init();
}

int64_t apic_mask(uint64_t irq) {
    ioapic_disable((uint8_t)irq);

    return 0;
}

int64_t apic_unmask(uint64_t irq) {
    ioapic_enable((uint8_t)irq);

    return 0;
}

int64_t apic_install(uint64_t irq, uint64_t arg) {
    ioapic_add(irq, arg);
    return 0;
}

int64_t apic_ack(uint64_t irq) {
    send_eoi((uint32_t)irq);
    return 0;
}

// export 供initctl 用
intctl_t apic_controller = {
    ._mask    = apic_mask,
    ._unmask  = apic_unmask,
    ._install = apic_install,
    .send_eoi = apic_ack,
};
