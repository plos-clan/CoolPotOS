#include "cpu/description_table.h"
#include "cpu_local.h"
#include "smp_x64.h"
#include "cpu/fsgsbase.h"
#include "mem/frame.h"

static __attribute__((naked)) void _setcs_helper() {
    __asm__ volatile("pop %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rax\n\t"
                     "lretq\n\t" ::
                         : "memory");
}

static void apu_gdt_setup() {
    cpu_local_t *this_cpu = get_current_cpu();

    x86_64_local_info_t *arch_local = this_cpu->arch_local_info;

    arch_local->gdtEntries[0] = 0x0000000000000000U;
    arch_local->gdtEntries[1] = 0x00a09a0000000000U;
    arch_local->gdtEntries[2] = 0x00c0920000000000U;
    arch_local->gdtEntries[3] = 0x00c0f20000000000U;
    arch_local->gdtEntries[4] = 0x00a0fa0000000000U;

    arch_local->gdt_pointer = ((struct gdt_register){
        .size = ((uint16_t)((uint32_t)sizeof(gdt_entries_t) - 1U)),
        .ptr  = &arch_local->gdtEntries,
    });

    __asm__ volatile("lgdt %[ptr]\n\t"
                     "call *%%rax\n\t"
                     "mov %[dseg], %%ds\n\t"
                     "mov %[dseg], %%fs\n\t"
                     "mov %[dseg], %%gs\n\t"
                     "mov %[dseg], %%es\n\t"
                     "mov %[dseg], %%ss\n\t"
                     :
                     : [ptr] "m"(arch_local->gdt_pointer),
                       [dseg] "rm"((uint16_t)0x10U),
                       "a"(&_setcs_helper),
                       "b"((uint16_t)0x8U)
                     : "memory");

    uint64_t address     = (uint64_t)&(arch_local->tss0);
    uint64_t low_base    = (address & 0xffffffU) << 16U;
    uint64_t mid_base    = (address >> 24U & 0xffU) << 56U;
    uint64_t high_base   = address >> 32U;
    uint64_t access_byte = (uint64_t)0x89U << 40U;
    uint64_t limit       = (uint32_t)(sizeof(tss_t) - 1U);

    arch_local->gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    arch_local->gdtEntries[6] = high_base;

    arch_local->tss0.ist[0] =
        (uint64_t)phys_to_virt(alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE) + KERNEL_STACK_SIZE);
    arch_local->tss0.ist[1] = arch_local->tss0.ist[0];
    arch_local->tss0.ist[2] = arch_local->tss0.ist[0];
    arch_local->tss0.ist[3] = arch_local->tss0.ist[0];
    arch_local->tss0.ist[4] = arch_local->tss0.ist[0];
    arch_local->tss0.ist[5] = arch_local->tss0.ist[0];
    arch_local->tss0.ist[6] = arch_local->tss0.ist[0];

    __asm__ volatile("ltr %[offset]\n\t" : : [offset] "rm"(0x28U) : "memory");

    write_gsbase((uint64_t)this_cpu);
}
