#include "../include/description_table.h"

extern void gdt_flush(uint32_t);
extern struct tss_entry tss_e;

gdt_entry_t gdt_entries[NGDT];
gdt_ptr_t   gdt_ptr;

void gdt_set_gate(uint8_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    gdt_entries[num].base_low = (base & 0xffff);
    gdt_entries[num].base_middle = (base >> 16) & 0xff;
    gdt_entries[num].base_high = (base >> 24) & 0xff;

    /* Setup the descriptor limits */
    gdt_entries[num].limit_low = (limit & 0xffff);
    gdt_entries[num].limit_high = ((limit >> 16) & 0x0f);

    /* Finally, set up the granularity and access flags */
    gdt_entries[num].flags = flags;

    access |= AC_RE; // 设置保留位为1
    gdt_entries[num].access = access;
}

void tss_init() {
    gdt_set_gate(SEL_TSS, (uint32_t)&tss_e, sizeof(struct tss_entry), AC_PR|AC_AC|AC_EX, GDT_GR); 
    /* for tss, access_reverse bit is 1 */
    gdt_entries[SEL_TSS].access &= ~AC_RE;
}


void install_gdt()
{
    gdt_ptr.limit = (sizeof(struct gdt_entry_struct) * NGDT) - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);                
    gdt_set_gate(SEL_KCODE, 0, 0xfffff, AC_RW|AC_EX|AC_DPL_KERN|AC_PR, GDT_GR|GDT_SZ); // 代码段
    gdt_set_gate(SEL_KDATA, 0, 0xfffff, AC_RW|AC_DPL_KERN|AC_PR, GDT_GR|GDT_SZ); // 数据段
    gdt_set_gate(SEL_UCODE, 0, 0xfffff, AC_RW|AC_EX|AC_DPL_USER|AC_PR, GDT_GR|GDT_SZ); // 用户代码段
    gdt_set_gate(SEL_UDATA, 0, 0xfffff, AC_RW|AC_DPL_USER|AC_PR, GDT_GR|GDT_SZ); // 用户数据段

    gdt_set_gate(SEL_SCODE, 0, 0xfffff, AC_RW|AC_EX|AC_DPL_SYST|AC_PR, GDT_GR|GDT_SZ);
    gdt_set_gate(SEL_SDATA, 0, 0xfffff, AC_RW|AC_DPL_SYST|AC_PR, GDT_GR|GDT_SZ);

    tss_init();
    gdt_flush((uint32_t)&gdt_ptr);
}