#include "description_table.h"
#include "isr.h"
#include "krlibc.h"
#include "kprint.h"

struct idt_register idt_pointer;
struct idt_entry idt_entries[256];

__IRQHANDLER void empty_handler(interrupt_frame_t *frame){
    send_eoi();
    printk("Empty interrupt.\n");
}

void idt_setup() {
    idt_pointer.size = (uint16_t) sizeof(idt_entries) - 1;
    idt_pointer.ptr = &idt_entries;
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");

    for (int i = 0; i < 256; i++) {
        register_interrupt_handler(i, empty_handler,0,0x8E);
    }
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

