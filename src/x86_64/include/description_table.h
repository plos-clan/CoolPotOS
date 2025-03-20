#pragma once

#define SA_RPL3 3

#define SA_RPL_MASK 0xFFFC
#define SA_TI_MASK 0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

#include "ctype.h"

struct idt_register {
    uint16_t size;
    void *ptr;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_low; //处理函数指针低16位地址
    uint16_t selector; //段选择子
    uint8_t ist;
    uint8_t flags; //标志位
    uint16_t offset_mid; //处理函数指针中16位地址
    uint32_t offset_hi;//处理函数指针高32位地址
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
