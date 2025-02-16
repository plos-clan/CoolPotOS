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
extern struct gdt_register gdt_pointer; //gdt.c
extern int now_pid; //pcb.c
extern pcb_t kernel_head_task; //scheduler.c
extern bool x2apic_mode; //apic.c

ticketlock apu_lock;

uint64_t *apu_stack[MAX_CPU];
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
}

void apu_entry(){
    __asm__ volatile("lidt %0" : : "m"(idt_pointer));

    apu_gdt_setup();

    page_table_t *physical_table = virt_to_phys((uint64_t)get_kernel_pagedir()->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));

    ticket_lock(&apu_lock);

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
    stbsp_sprintf(name,"CP_IDLE_CPU%lu",get_current_cpuid());
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
        cpus[info->processor_id].kernel_stack = (uint64_t)malloc(32768);
        cpus[info->processor_id].flags = 1;
        info->goto_address = (void*)apu_entry;
    }
    kinfo("%d processors have been enabled.",cpu_count);
}
