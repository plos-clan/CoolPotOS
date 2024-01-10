#include "../include/gdt.h"
#include "../include/task.h"
#include "../include/common.h"
#include "../include/memory.h"

extern gdt_flush(uint32_t);

extern void tss_flush();;

gdt_entry_t gdt_entries[5]; // 总共只有5个描述符
gdt_ptr_t gdt_ptr;
tss_entry_t tss_entry;

static void write_tss(s32int num, u16int ss0, u32int esp0) {
    u32int base = (u32int) &tss_entry;
    u32int limit = base + sizeof(tss_entry);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;

    tss_entry.cs = 0x0b;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
}

void gdt_install() {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base = (u32int) &gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment
    write_tss(5, 0x10, 0x0);

    gdt_flush((u32int) &gdt_ptr);
}

void set_kernel_stack(u32int stack) {
    tss_entry.esp0 = stack;
}

void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) // 设置单个GDT描述符
{
    gdt_entries[num].base_low = (base & 0xffff);
    gdt_entries[num].base_mid = (base >> 16) & 0xff;
    gdt_entries[num].base_high = (base >> 24) & 0xff; // 拆成低 中 高 三个部分

    gdt_entries[num].limit_low = (limit & 0xffff);
    gdt_entries[num].gbit = (limit >> 16) & 0x0f; // limit_mid和gbit混在一块了，很怪

    gdt_entries[num].gbit |= gran & 0xF0; // 把gbit也补上
    gdt_entries[num].access = access;
}
