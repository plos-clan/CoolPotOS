#include "task/smp.h"
#include "description_table.h"
#include "io.h"
#include "apic.h"
#include "fsgsbase.h"
#include "mem/page.h"
#include "mem/frame.h"
#include "krlibc.h"

extern struct idt_register idt_pointer;

static __attr(naked) void _setcs_helper() {
    __asm__ volatile("pop %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rax\n\t"
                     "lretq\n\t" ::
                         : "memory");
}

static void apu_gdt_setup() {
    uint32_t   this_id  = lapic_id();
    cpu_local_t *this_cpu = get_cpu_local(this_id);

    this_cpu->arch_data.gdtEntries[0] = 0x0000000000000000U;
    this_cpu->arch_data.gdtEntries[1] = 0x00a09a0000000000U;
    this_cpu->arch_data.gdtEntries[2] = 0x00c0920000000000U;
    this_cpu->arch_data.gdtEntries[3] = 0x00c0f20000000000U;
    this_cpu->arch_data.gdtEntries[4] = 0x00a0fa0000000000U;

    this_cpu->arch_data.gdt_pointer = ((struct gdt_register){
        .size = ((uint16_t)((uint32_t)sizeof(gdt_entries_t) - 1U)),
        .ptr  = &this_cpu->arch_data.gdtEntries,
    });

    __asm__ volatile("lgdt %[ptr]\n\t"
                     "call *%%rax\n\t"
                     "mov %[dseg], %%ds\n\t"
                     "mov %[dseg], %%fs\n\t"
                     "mov %[dseg], %%gs\n\t"
                     "mov %[dseg], %%es\n\t"
                     "mov %[dseg], %%ss\n\t"
                     :
                     : [ptr] "m"(this_cpu->arch_data.gdt_pointer), [dseg] "rm"((uint16_t)0x10U),
                       "a"(&_setcs_helper), "b"((uint16_t)0x8U)
                     : "memory");

    write_fsbase(0);
    write_gsbase((uint64_t)this_cpu);
    write_kgsbase((uint64_t)this_cpu);
    this_cpu->id = this_id;

    uint64_t address     = (uint64_t)&(this_cpu->arch_data.tss0);
    uint64_t low_base    = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base    = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base   = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit       = ((uint64_t)(uint32_t)(sizeof(tss_t) - 1U));

    this_cpu->arch_data.gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    this_cpu->arch_data.gdtEntries[6] = high_base;

    this_cpu->arch_data.tss0.ist[0] = ((uint64_t)&(this_cpu->arch_data.tss_stack)) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset]\n\t" : : [offset] "rm"(0x28U) : "memory");
}

_Noreturn void arch_ap_cpu_entry(){
    page_table_t *physical_table = (page_table_t*)virt_to_phys(get_kernel_pagedir()->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
    apu_gdt_setup();
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
    ap_local_apic_init();
    while (true) arch_wait_for_interrupt();
}