#include "description_table.h"

struct idt_register idt_pointer;
struct idt_entry idt_entries[256];

void idt_setup() {
    idt_pointer.size = (uint16_t) sizeof(idt_entries) - 1;
    idt_pointer.ptr = &idt_entries;

    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
}

void register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags) {
    uint64_t addr = (uint64_t) handler;
    idt_entries[vector].offset_low = (uint16_t) addr;
    idt_entries[vector].ist = ist;
    idt_entries[vector].flags = flags;
    idt_entries[vector].selector = 0x08;
    idt_entries[vector].offset_mid = (uint16_t) (addr >> 16);
    idt_entries[vector].offset_hi = (uint32_t) (addr >> 32);
}

