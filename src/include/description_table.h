#pragma once

#include "ctype.h"

struct idt_register {
    uint16_t size;
    void *ptr;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_register {
    uint16_t size;
    void *ptr;
} __attribute__((packed));

struct tss {
    uint32_t unused0;
    uint64_t rsp[3];
    uint64_t unused1;
    uint64_t ist[7];
    uint64_t unused2;
    uint16_t unused3;
    uint16_t iopb;
} __attribute__((packed));

typedef struct tss tss_t;
typedef uint64_t gdt_entries_t[7];
typedef uint8_t tss_stack_t[1024];

void gdt_setup();

void tss_setup();

void idt_setup();
