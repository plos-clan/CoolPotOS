#include "description_table.h"
#include "krlibc.h"

gdt_entry_t gdt_entries[GDT_LENGTH];
gdt_ptr_t gdt_ptr;

tss_entry tss;

void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
    uintptr_t base = (uintptr_t)&tss;
    uintptr_t limit = base + sizeof(tss);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    memset(&tss, 0x0, sizeof(tss));

    tss.ss0 = ss0;
    tss.esp0 = esp0;
    tss.cs = 0x0b;
    tss.ss = 0x13;
    tss.ds = 0x13;
    tss.es = 0x13;
    tss.fs = 0x13;
    tss.gs = 0x13;

    tss.iomap_base = sizeof(tss);
}

void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low     = (base & 0xFFFF);
    gdt_entries[num].base_middle  = (base >> 16) & 0xFF;
    gdt_entries[num].base_high    = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low    = (limit & 0xFFFF);
    gdt_entries[num].granularity  = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access       = access;
}

void set_kernel_stack(uintptr_t stack) {
    tss.esp0 = stack;
}

void gdt_install() {
    gdt_ptr.limit = sizeof(gdt_entry_t) * GDT_LENGTH - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);   // 按照 Intel 文档要求，第一个描述符必须全 0
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);     // 指令段
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);     // 数据段
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);     // 用户模式代码段
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);     // 用户模式数据段

    gdt_flush((uint32_t)&gdt_ptr);

    uint32_t esp;
    __asm__ volatile ("mov %%esp, %0" : "=r"(esp));

    write_tss(5, 0x10, esp);
    tss_flush();
}
