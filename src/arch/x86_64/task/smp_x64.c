#include "apic.h"
#include "description_table.h"
#include "fpu.h"
#include "fsgsbase.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "syscall.h"
#include "security.h"

extern struct idt_register idt_pointer;

static __attr(naked) void _setcs_helper() {
    __asm__ volatile("pop %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rax\n\t"
                     "lretq\n\t" ::
                         : "memory");
}

static void apu_gdt_setup() {
    uint32_t     this_id  = lapic_id();
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

    write_gsbase((uint64_t)this_cpu);
    write_kgsbase((uint64_t)this_cpu);

    uint64_t address     = (uint64_t)&(this_cpu->arch_data.tss0);
    uint64_t low_base    = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base    = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base   = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit       = ((uint64_t)(uint32_t)(sizeof(tss_t) - 1U));

    this_cpu->arch_data.gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    this_cpu->arch_data.gdtEntries[6] = high_base;

    this_cpu->arch_data.tss0.ist[0] =
        ((uint64_t)&(this_cpu->arch_data.tss_stack)) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset]\n\t" : : [offset] "rm"(0x28U) : "memory");
}

void arch_bsp_cpu_init() {
    uint32_t     this_id  = lapic_id();
    cpu_local_t *this_cpu = get_cpu_local(this_id);
    write_gsbase((uint64_t)this_cpu);
    write_kgsbase((uint64_t)this_cpu);
}

cpu_local_t *arch_current_cpu() {
    return get_cpu_local(lapic_id());
}

_Noreturn void arch_ap_cpu_entry() {
    init_stack_canary();

    page_table_t *physical_table = (page_table_t *)virt_to_phys(get_kernel_pagedir()->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
    apu_gdt_setup();
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
    ap_local_apic_init();
    calibrate_tsc_with_hpet();

    extern pcb_t kernel_process;
    tcb_t        idle_thread = malloc(STACK_SIZE);
    idle_thread->process     = kernel_process;
    idle_thread->tid         = alloc_tid();
    idle_thread->ct_index    = cow_list_add(kernel_process->child_threads, idle_thread);
    idle_thread->status      = T_RUNNING;
    set_cpu_idle_task(idle_thread, arch_current_cpu());
    float_processor_setup();
    arch_context_init(&idle_thread->context);
    arch_enable_syscall();
    arch_open_interrupt();

    while (true)
        arch_wait_for_interrupt();
}