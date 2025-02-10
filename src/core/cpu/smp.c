#include "smp.h"
#include "krlibc.h"
#include "kprint.h"
#include "description_table.h"
#include "io.h"
#include "hhdm.h"
#include "limine.h"

extern struct idt_register idt_pointer; //idt.c
extern struct gdt_register gdt_pointer; //gdt.c
extern bool x2apic_mode; //apic.c

uint64_t cpu_count = 0;

uint64_t cpu_num(){
    return cpu_count;
}

void apu_entry(){
    __asm__ volatile (
            "lgdt %[ptr];"
            "push %[cseg];"
            "lea 1f, %%rax;"
            "push %%rax;"
            "lretq ;"
            "1:"
            "mov %[dseg], %%ds;"
            "mov %[dseg], %%fs;"
            "mov %[dseg], %%gs;"
            "mov %[dseg], %%es;"
            "mov %[dseg], %%ss;"
            : : [ptr] "m"(gdt_pointer),
    [cseg] "rm"((uint16_t) 0x8U),
    [dseg] "rm"((uint16_t) 0x10U)
    : "memory"
    );
    __asm__ volatile("lidt %0" : : "m"(idt_pointer));


    uint64_t ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= 1 << 11;
    if (x2apic_mode){
        ia32_apic_base |= 1 << 10;
    }
    wrmsr(0x1b, ia32_apic_base);
    local_apic_init(false);
    cpu_hlt;
}

void apu_startup(struct limine_smp_request smp_request){
    struct limine_smp_response *response = smp_request.response;
    cpu_count = response->cpu_count;
    for (uint64_t i = 0; i < cpu_count; i++){
        struct limine_smp_info *info = response->cpus[i];
        if(info->lapic_id == response->bsp_lapic_id) continue;
        info->goto_address = (void*)apu_entry;
    }
    kinfo("%d processors have been enabled.",cpu_count);
}
