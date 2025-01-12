#include "acpi.h"
#include "kprint.h"
#include "hhdm.h"
#include "io.h"

uint32_t lapic_address;
uint32_t ioapic_address;

void disable_pic() {
    io_out8(0x21, 0xff);
    io_out8(0xa1, 0xff);
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
    kinfo("Local APIC address: 0x%p", lapic_address);
    kinfo("IO APIC address: 0x%p", ioapic_address);
    disable_pic();
}
