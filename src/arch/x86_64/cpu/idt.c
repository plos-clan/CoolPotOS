#include "description_table.h"
#include "isr.h"
#include "kprint.h"
#include "krlibc.h"

struct idt_register idt_pointer;
struct idt_entry    idt_entries[256];

extern void (*empty_handle[256])(interrupt_frame_t *frame);

void idt_setup() {
    idt_pointer.size = (uint16_t)sizeof(idt_entries) - 1;
    idt_pointer.ptr  = &idt_entries;
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");

    for (int i = 0; i < 256; i++) {
        register_interrupt_handler(i, (void *)(empty_handle[i]), 0, 0x8E);
    }
}

void register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags) {
    uint64_t addr                  = (uint64_t)handler;
    idt_entries[vector].offset_low = (uint16_t)addr;
    idt_entries[vector].ist        = ist;
    idt_entries[vector].flags      = flags;
    idt_entries[vector].selector   = 0x08;
    idt_entries[vector].offset_mid = (uint16_t)(addr >> 16);
    idt_entries[vector].offset_hi  = (uint32_t)(addr >> 32);
}
