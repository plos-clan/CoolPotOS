#include "acpi.h"
#include "kprint.h"
#include "hhdm.h"
#include "io.h"
#include "limine.h"
#include "timer.h"
#include "isr.h"

bool x2apic_mode;
uint64_t lapic_address;
uint64_t ioapic_address;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags = 1U,
};

void disable_pic() {
    io_out8(0x21, 0xff);
    io_out8(0xa1, 0xff);
}

static void ioapic_write(uint32_t reg,uint32_t value) {
    mmio_write32((uint32_t*)((uint64_t)ioapic_address), reg);
    mmio_write32((uint32_t*)((uint64_t)ioapic_address + 0x10), value);
}

static uint32_t ioapic_read(uint32_t reg){
    mmio_write32((uint32_t*)((uint64_t)ioapic_address), reg);
    return mmio_read32((uint32_t*)((uint64_t)ioapic_address + 0x10));
}

void ioapic_add(uint8_t vector, uint32_t irq) {
    uint32_t ioredtbl = (uint32_t)(0x10 + (uint32_t)(irq * 2));
    uint64_t redirect = (uint64_t )vector;
    redirect |= lapic_id() << 56;
    ioapic_write(ioredtbl, (uint32_t)redirect);
    ioapic_write(ioredtbl + 1, (uint32_t)(redirect >> 32));
}

static void lapic_write(uint32_t reg, uint32_t value) {
    if(x2apic_mode){
        wrmsr(0x800 + (reg >> 4), value);
        return;
    }
    mmio_write32((uint32_t*)((uint64_t)lapic_address + reg), value);
}

static uint32_t lapic_read(uint32_t reg) {
    if(x2apic_mode){
        return rdmsr(0x800 + (reg >> 4));
    }
    return mmio_read32((uint32_t*)((uint64_t)lapic_address + reg));
}

uint64_t lapic_id() {
    return (lapic_read(LAPIC_REG_ID) >> 24);
}

void local_apic_init() {
    x2apic_mode = (smp_request.flags & 1U) != 0;
    lapic_write(LAPIC_REG_TIMER, timer);
    lapic_write(LAPIC_REG_SPURIOUS, 0xff | 1 << 8);
    lapic_write(LAPIC_REG_TIMER_DIV, 11);
    lapic_write(LAPIC_REG_TIMER_INITCNT, 0);

    uint64_t b = nanoTime();
    lapic_write(LAPIC_REG_TIMER_INITCNT, ~((uint32_t) 0));
    for (;;) if (nanoTime() - b >= 1000000) break;
    uint64_t lapic_timer = (~(uint32_t) 0) - lapic_read(LAPIC_REG_TIMER_CURCNT);
    uint64_t calibrated_timer_initial = (uint64_t) ((uint64_t) (lapic_timer * 1000) / 250);
    printk("Calibrated LAPIC timer: %d ticks per second.\n", calibrated_timer_initial);
    lapic_write(LAPIC_REG_TIMER, lapic_read(LAPIC_REG_TIMER) | 1 << 17);
    lapic_write(LAPIC_REG_TIMER_INITCNT,calibrated_timer_initial);
    kinfo("Setup local %s.",x2apic_mode ? "x2APIC" : "APIC");
}

void io_apic_init(){
    ioapic_address = (uint64_t)phys_to_virt(ioapic_address);
    ioapic_add((uint8_t)timer,0);
    ioapic_add((uint8_t)keyboard,1);
    kinfo("Setup I/O apic.", ioapic_address);
}

void send_eoi(){
    lapic_write(0xb0,0);
}

void apic_setup(MADT *madt) {
    lapic_address = madt->local_apic_address;
    uint64_t current = 0;
    for (;;) {
        if (current + ((uint32_t) sizeof(MADT) - 1) >= madt->h.Length) {
            break;
        }
        MadtHeader *header = (MadtHeader *) ((uint64_t) (&madt->entries) + current);
        if (header->entry_type == MADT_APIC_IO) {
            MadtIOApic *ioapic = (MadtIOApic *) ((uint64_t) (&madt->entries) + current);
            ioapic_address = ioapic->address;
        }
        current += (uint64_t) header->length;
    }
    disable_pic();
    local_apic_init();
    io_apic_init();
}
