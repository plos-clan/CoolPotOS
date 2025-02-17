#include "smp.h"
#include "krlibc.h"
#include "kprint.h"
#include "description_table.h"
#include "io.h"
#include "hhdm.h"
#include "limine.h"
#include "lock.h"
#include "pcb.h"
#include "alloc.h"
#include "sprintf.h"
#include "pivfs.h"

extern struct idt_register idt_pointer; //idt.c
extern int now_pid; //pcb.c
extern pcb_t kernel_head_task; //scheduler.c
extern bool x2apic_mode; //apic.c

ticketlock apu_lock;

smp_cpu_t cpus[MAX_CPU];

uint64_t cpu_count = 0;

uint64_t cpu_num(){
    return cpu_count;
}

uint64_t get_current_cpuid(){
    uint64_t rax, rbx, rcx, rdx;
    rax = 0xB;
    rcx = 0;
    __asm__ volatile (
            "cpuid"
            : "=a"(rax), "=b"(rbx), "=c"(rcx), "=d"(rdx)
            : "a"(rax), "c"(rcx)
            );
    return rdx;
}

static void apu_gdt_setup(){
    smp_cpu_t cpu = cpus[get_current_cpuid()];
    cpu.gdtEntries[0] = 0x0000000000000000U;
    cpu.gdtEntries[1] = 0x00a09a0000000000U;
    cpu.gdtEntries[2] = 0x00c0920000000000U;
    cpu.gdtEntries[3] = 0x00c0f20000000000U;
    cpu.gdtEntries[4] = 0x00a0fa0000000000U;

    cpu.gdt_pointer = ((struct gdt_register) {
            .size = ((uint16_t) ((uint32_t) sizeof(gdt_entries_t) - 1U)),
            .ptr = &cpu.gdtEntries,
    });

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
            : : [ptr] "m"(cpu.gdt_pointer),
    [cseg] "rm"((uint16_t) 0x8U),
    [dseg] "rm"((uint16_t) 0x10U)
    : "memory"
    );

    uint64_t address = ((uint64_t)(&cpu.tss0));
    uint64_t low_base = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit = ((uint64_t)((uint32_t)(sizeof(tss_t) - 1U)));
    cpu.gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    cpu.gdtEntries[6] = high_base;

    cpu.tss0.ist[0] = ((uint64_t) &cpu.tss_stack) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset];" : : [offset]"rm"(0x28U) : "memory");
}

void apu_entry(){
    __asm__ volatile("lidt %0" : : "m"(idt_pointer));

    ticket_lock(&apu_lock);
    apu_gdt_setup();

    page_table_t *physical_table = virt_to_phys((uint64_t)get_kernel_pagedir()->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));

    uint64_t ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= 1 << 11;
    if (x2apic_mode){
        ia32_apic_base |= 1 << 10;
    }
    wrmsr(0x1b, ia32_apic_base);
    local_apic_init(false);

    pcb_t apu_idle = (pcb_t)malloc(STACK_SIZE);
    apu_idle->task_level = 0;
    apu_idle->pid = now_pid++;
    apu_idle->cpu_clock = 0;
    apu_idle->directory = get_kernel_pagedir();
    //TODO 核心私有TSS未实现 set_kernel_stack(get_rsp());
    apu_idle->kernel_stack = apu_idle->context0.rsp = get_rsp();
    apu_idle->user_stack = apu_idle->kernel_stack;
    apu_idle->tty = get_default_tty();
    apu_idle->context0.rflags = get_rflags();
    apu_idle->cpu_timer = nanoTime();
    apu_idle->time_buf = alloc_timer();
    apu_idle->cpu_id = get_current_cpuid();
    char name[50];
    sprintf(name,"CP_IDLE_CPU%lu",get_current_cpuid());
    memcpy(apu_idle->name, name, strlen(name));
    apu_idle->next = kernel_head_task;
    add_task(apu_idle);
    pivfs_update(kernel_head_task);
    ticket_unlock(&apu_lock);
    cpu_hlt;
}

void apu_startup(struct limine_smp_request smp_request){
    struct limine_smp_response *response = smp_request.response;
    cpu_count = response->cpu_count;
    for (uint64_t i = 0; i < cpu_count && i < MAX_CPU; i++){
        struct limine_smp_info *info = response->cpus[i];
        if(info->lapic_id == response->bsp_lapic_id) continue;
        cpus[info->processor_id].lapic_id = info->lapic_id;
        cpus[info->processor_id].flags = 1;
        info->goto_address = (void*)apu_entry;
    }
    kinfo("%d processors have been enabled.",cpu_count);
}
