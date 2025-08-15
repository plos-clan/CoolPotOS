#include "apic.h"
#include "acpi.h"
#include "boot.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "timer.h"

bool     x2apic_mode;
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

static void ioapic_mmio_write(uintptr_t base, uint32_t reg, uint32_t value) {
    mmio_write32((void *)base, reg);
    mmio_write32((void *)(base + 0x10), value);
}

static uint32_t ioapic_mmio_read(uintptr_t base, uint32_t reg) {
    mmio_write32((void *)base, reg);
    return mmio_read32((void *)(base + 0x10));
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

    uint32_t irq0     = irq - io->gsi_base;
    uint32_t ioredtbl = 0x10 + irq0 * 2;
    uint64_t redirect = vector | ((uint64_t)lapic_id() << 56);

    ioapic_mmio_write(io->mmio_base, ioredtbl, (uint32_t)redirect);
    ioapic_mmio_write(io->mmio_base, ioredtbl + 1, (uint32_t)(redirect >> 32));
}

static void lapic_write(uint32_t reg, uint32_t value) {
    if (x2apic_mode) {
        wrmsr(0x800 + (reg >> 4), value);
        return;
    }
    mmio_write32((uint32_t *)((uint64_t)lapic_address + reg), value);
}

uint32_t lapic_read(uint32_t reg) {
    if (x2apic_mode) { return rdmsr(0x800 + (reg >> 4)); }
    return mmio_read32((uint32_t *)((uint64_t)lapic_address + reg));
}

uint64_t lapic_id() {
    uint32_t phy_id = lapic_read(LAPIC_REG_ID);
    return x2apic_mode ? phy_id : (phy_id >> 24);
}

uint32_t isa_irq_to_gsi(uint8_t isa_irq) {
    for (size_t i = 0; i < found_iso_count; i++) {
        if (found_isos[i].irq_source == isa_irq) { return found_isos[i].gsi; }
    }
    return isa_irq;
}

void ap_local_apic_init() {
    uint64_t value  = rdmsr(0x1b);
    value          |= (1UL << 11);
    if (x2apic_mode) value |= (1UL << 10);
    wrmsr(0x1b, value);

    lapic_timer_stop();

    lapic_write(LAPIC_REG_SPURIOUS, 0xff | (1 << 8));
    lapic_write(LAPIC_REG_TIMER_DIV, 11);
    lapic_write(LAPIC_REG_TIMER, timer);

    lapic_write(LAPIC_REG_TIMER, lapic_read(LAPIC_REG_TIMER) | (1 << 17));
    lapic_write(LAPIC_REG_TIMER_INITCNT, calibrated_timer_initial);
}

void local_apic_init() {
    x2apic_mode = x2apic_mode_supported();

    uint64_t data  = rdmsr(0x1b);
    data          |= 1UL << 11;
    if (x2apic_mode) { data |= 1UL << 10; }
    wrmsr(0x1b, data);

    lapic_timer_stop();

    lapic_write(LAPIC_REG_SPURIOUS, 0xff | 1 << 8);
    lapic_write(LAPIC_REG_TIMER_DIV, 11);
    lapic_write(LAPIC_REG_TIMER, timer);

    uint64_t b = elapsed();
    lapic_write(LAPIC_REG_TIMER_INITCNT, ~((uint32_t)0));
    for (;;)
        if (elapsed() - b >= 1000000) break;
    uint64_t lapic_timer     = (~(uint32_t)0) - lapic_read(LAPIC_REG_TIMER_CURCNT);
    calibrated_timer_initial = (uint64_t)((uint64_t)(lapic_timer * 1000) / LAPIC_TIMER_SPEED);
    lapic_write(LAPIC_REG_TIMER, lapic_read(LAPIC_REG_TIMER) | 1 << 17);
    lapic_write(LAPIC_REG_TIMER_INITCNT, calibrated_timer_initial);

    kinfo("Calibrated LAPIC timer: %d ticks per second.", calibrated_timer_initial);
    kinfo("Setup local %s.", x2apic_mode ? "x2APIC" : "APIC");
}

__IRQHANDLER static void empty_interrupt_handle_apic(interrupt_frame_t *frame) {
    send_eoi();
}

void ioapic_redirect_irq(uint8_t vector, uint32_t irq) {
    uint32_t gsi = isa_irq_to_gsi((uint8_t)irq);
    ioapic_add(vector, gsi);
    ioapic_enable(vector, gsi);
}

void io_apic_init() {
    ioapic_redirect_irq((uint8_t)timer, 0);
    ioapic_redirect_irq((uint8_t)keyboard, 1);
    ioapic_redirect_irq((uint8_t)sb16, 5);
    ioapic_redirect_irq((uint8_t)mouse, 12);
    ioapic_redirect_irq((uint8_t)ide_primary, 14);
    ioapic_redirect_irq((uint8_t)ide_secondary, 15);

    register_interrupt_handler(ide_primary, empty_interrupt_handle_apic, 0, 0x8E);
    register_interrupt_handler(ide_secondary, empty_interrupt_handle_apic, 0, 0x8E);
    kinfo("Setup I/O apic.");
}

void ioapic_enable(uint8_t vector, uint32_t irq) {
    struct ioapic_info *io = find_ioapic(irq);
    if (!io) return;

    uint64_t index = 0x10 + ((vector - 32) * 2);
    uint64_t value = (uint64_t)ioapic_mmio_read(io->mmio_base, index + 1) << 32 |
                     (uint64_t)ioapic_mmio_read(io->mmio_base, index);
    value &= (~0x10000UL);
    ioapic_mmio_write(io->mmio_base, index, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_mmio_write(io->mmio_base, index + 1, (uint32_t)(value >> 32));
}

void send_eoi() {
    lapic_write(0xb0, 0);
}

void lapic_timer_stop() {
    lapic_write(LAPIC_REG_TIMER_INITCNT, 0);
    lapic_write(LAPIC_REG_TIMER, (1 << 16));
}

void send_ipi(uint32_t apic_id, uint32_t command) {
    if (x2apic_mode) {
        lapic_write(APIC_ICR_LOW, (((uint64_t)apic_id) << 32) | command);
    } else {
        lapic_write(APIC_ICR_HIGH, apic_id << 24);
        lapic_write(APIC_ICR_LOW, command);
    }
}

void apic_setup(MADT *madt) {
    lapic_address = (uint64_t)phys_to_virt(madt->local_apic_address);

    uint64_t current = 0;
    for (;;) {
        if (current + ((uint32_t)sizeof(MADT) - 1) >= madt->h.Length) { break; }
        MadtHeader *header = (MadtHeader *)((uint64_t)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_IO) {
            MadtIOApic *ioapic             = (MadtIOApic *)((uint64_t)(&madt->entries) + current);
            size_t      count              = found_ioapic_count++;
            found_ioapics[count].id        = ioapic->apic_id;
            found_ioapics[count].mmio_base = (uint64_t)phys_to_virt(ioapic->address);
            found_ioapics[count].gsi_base  = ioapic->gsib;

            uint32_t ver                   = ioapic_mmio_read(found_ioapics[count].mmio_base, 1);
            found_ioapics[count].irq_count = ((ver >> 16) & 0xFF) + 1;
            kinfo("Found IOAPIC ID: %d, MMIO: 0x%x, GSI Base: %d, IRQ: %d", ioapic->apic_id,
                  ioapic->address, ioapic->gsib, found_ioapics[count].irq_count);
        } else if (header->entry_type == MADT_APIC_INT) {
            MadtInterruptSource *iso =
                (MadtInterruptSource *)((uint64_t)(&madt->entries) + current);
            size_t c                 = found_iso_count++;
            found_isos[c].bus        = iso->bus_source;
            found_isos[c].irq_source = iso->irq_source;
            found_isos[c].gsi        = iso->gsi;
            found_isos[c].flags      = iso->flags;
            logkf("Found Interrupt Source Bus: %d, IRQ Source: %d, GSI: %d, Flags: 0x%x\n\r",
                  iso->bus_source, iso->irq_source, iso->gsi, iso->flags);
        }
        current += (uint64_t)header->length;
    }
    disable_pic();
    local_apic_init();
    io_apic_init();
}