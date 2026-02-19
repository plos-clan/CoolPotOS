#include "description_table.h"
#include "term/klog.h"

static struct idt_register idt_pointer;
static struct idt_entry idt_entries[256];

struct idt_register *get_idt_register() {
    return &idt_pointer;
}

void idt_setup() {
    idt_pointer.size = (uint16_t)sizeof(idt_entries) - 1;
    idt_pointer.ptr  = &idt_entries;
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
    kinfo("Setup interrupt table - lidt:%p", idt_pointer);
}

void register_interrupt_handler(
    const uint16_t vector, void *handler, const uint8_t ist, const uint8_t flags
) {
    const uint64_t addr            = (uint64_t)handler;
    idt_entries[vector].offset_low = (uint16_t)addr;
    idt_entries[vector].ist        = ist;
    idt_entries[vector].flags      = flags;
    idt_entries[vector].selector   = 0x08;
    idt_entries[vector].offset_mid = (uint16_t)(addr >> 16);
    idt_entries[vector].offset_hi  = (uint32_t)(addr >> 32);
}
