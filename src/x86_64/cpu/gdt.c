#include "boot.h"
#include "description_table.h"
#include "fsgsbase.h"
#include "io.h"
#include "krlibc.h"
#include "smp.h"

gdt_entries_t       gdt_entries;
struct gdt_register gdt_pointer;
tss_t               tss0;
tss_stack_t         tss_stack;
extern uint32_t     bsp_processor_id;

static __attr(naked) void _setcs_helper() {
    __asm__ volatile("pop %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rax\n\t"
                     "lretq\n\t" ::
                         : "memory");
}

void gdt_setup() {
    gdt_entries[0] = 0x0000000000000000U;
    gdt_entries[1] = 0x00a09a0000000000U;
    gdt_entries[2] = 0x00c0920000000000U;
    gdt_entries[3] = 0x00c0f20000000000U;
    gdt_entries[4] = 0x00a0fa0000000000U;

    gdt_pointer = ((struct gdt_register){
        .size = ((uint16_t)((uint32_t)sizeof(gdt_entries_t) - 1U)),
        .ptr  = &gdt_entries,
    });

    __asm__ volatile("lgdt %[ptr]\n\t"
                     "call *%%rax\n\t"
                     "mov %[dseg], %%ds\n\t"
                     "mov %[dseg], %%fs\n\t"
                     "mov %[dseg], %%gs\n\t"
                     "mov %[dseg], %%es\n\t"
                     "mov %[dseg], %%ss\n\t"
                     :
                     : [ptr] "m"(gdt_pointer), [dseg] "rm"((uint16_t)0x10U), "a"(&_setcs_helper),
                       "b"((uint16_t)0x8U)
                     : "memory");

    uint32_t   boot_id  = get_smp_info()->bsp_lapic_id;
    smp_cpu_t *boot_cpu = &smp_cpus[boot_id];
    wrmsr(0xC0000100, 0);
    wrmsr(0xC0000101, (uint64_t)boot_cpu);
    wrmsr(0xC0000102, (uint64_t)boot_cpu);
    current_cpu->id = boot_id;

    tss_setup();
}

void tss_setup() {
    uint64_t address     = ((uint64_t)(&tss0));
    uint64_t low_base    = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base    = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base   = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit       = ((uint64_t)((uint32_t)(sizeof(tss_t) - 1U)));
    gdt_entries[5]       = (((low_base | mid_base) | limit) | access_byte);
    gdt_entries[6]       = high_base;

    tss0.ist[0] = ((uint64_t)&tss_stack) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset];" : : [offset] "rm"(0x28U) : "memory");
}

void set_kernel_stack(uint64_t rsp) {
    uint64_t cpuid = current_cpu->id;
    if (cpuid == bsp_processor_id)
        tss0.rsp[0] = rsp;
    else { smp_cpus[cpuid].tss0.rsp[0] = rsp; }
}
