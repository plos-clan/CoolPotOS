#include "smp.h"
#include "alloc.h"
#include "description_table.h"
#include "fpu.h"
#include "hhdm.h"
#include "io.h"
#include "kprint.h"
#include "krlibc.h"
#include "limine.h"
#include "lock.h"
#include "pcb.h"
#include "sprintf.h"

extern struct idt_register idt_pointer;      // idt.c
extern tcb_t               kernel_head_task; // scheduler.c
extern bool                x2apic_mode;      // apic.c
extern pcb_t               kernel_group;     // pcb.c
ticketlock                 apu_lock;

smp_cpu_t cpus[MAX_CPU];
uint32_t  bsp_processor_id;
uint64_t  cpu_count = 0;

static uint64_t cpu_done_count = 0;

static void apu_hlt() {
    cpu_hlt;
}

uint64_t cpu_num() {
    return cpu_count;
}

uint64_t get_current_cpuid() {
    uint64_t rax, rbx, rcx, rdx;
    rax = 0xB;
    rcx = 0;
    __asm__ volatile("cpuid" : "=a"(rax), "=b"(rbx), "=c"(rcx), "=d"(rdx) : "a"(rax), "c"(rcx));
    return rdx;
}

static void apu_gdt_setup() {
    smp_cpu_t *cpu = &cpus[get_current_cpuid()];

    cpu->gdtEntries[0] = 0x0000000000000000U;
    cpu->gdtEntries[1] = 0x00a09a0000000000U;
    cpu->gdtEntries[2] = 0x00c0920000000000U;
    cpu->gdtEntries[3] = 0x00c0f20000000000U;
    cpu->gdtEntries[4] = 0x00a0fa0000000000U;

    cpu->gdt_pointer = ((struct gdt_register){
        .size = ((uint16_t)((uint32_t)sizeof(gdt_entries_t) - 1U)),
        .ptr  = &cpu->gdtEntries,
    });

    __asm__ volatile(
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
        :
        : [ptr] "m"(cpu->gdt_pointer), [cseg] "rm"((uint16_t)0x8U), [dseg] "rm"((uint16_t)0x10U)
        : "memory");

    uint64_t address     = (uint64_t)&(cpu->tss0);
    uint64_t low_base    = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base    = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base   = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit       = ((uint64_t)(uint32_t)(sizeof(tss_t) - 1U));

    printk("", address); // 用于对抗编译器优化

    cpu->gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    cpu->gdtEntries[6] = high_base;

    cpu->tss0.ist[0] = ((uint64_t)&(cpu->tss_stack)) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset];" : : [offset] "rm"(0x28U) : "memory");
}

void apu_entry() {
    __asm__ volatile("lidt %0" : : "m"(idt_pointer));

    ticket_lock(&apu_lock);
    apu_gdt_setup();

    page_table_t *physical_table = virt_to_phys((uint64_t)get_kernel_pagedir()->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));

    uint64_t ia32_apic_base  = rdmsr(0x1b);
    ia32_apic_base          |= 1 << 11;
    if (x2apic_mode) { ia32_apic_base |= 1 << 10; }
    wrmsr(0x1b, ia32_apic_base);
    local_apic_init(false);

    float_processor_setup();

    tcb_t apu_idle       = (tcb_t)malloc(STACK_SIZE);
    not_null_assets(apu_idle);
    apu_idle->task_level = TASK_KERNEL_LEVEL;
    apu_idle->pid        = kernel_group->pid_index++;
    apu_idle->cpu_clock  = 0;
    set_kernel_stack(get_rsp());
    apu_idle->kernel_stack = apu_idle->context0.rsp = get_rsp();
    apu_idle->user_stack                            = apu_idle->kernel_stack;
    apu_idle->context0.rflags                       = get_rflags() | 0x200;
    apu_idle->cpu_timer                             = nanoTime();
    apu_idle->time_buf                              = alloc_timer();
    apu_idle->cpu_id                                = get_current_cpuid();
    apu_idle->context0.rip                          = (uint64_t)apu_hlt;
    char name[50];
    sprintf(name, "CP_IDLE_CPU%lu", get_current_cpuid());
    memcpy(apu_idle->name, name, strlen(name));
    apu_idle->name[strlen(name)] = '\0';
    smp_cpu_t *cpu               = &(cpus[get_current_cpuid()]);
    cpu->idle_pcb                = apu_idle;
    cpu->current_pcb             = apu_idle;
    cpu->flags                   = 1;
    apu_idle->queue_index        = queue_enqueue(cpu->scheduler_queue, apu_idle);
    if (apu_idle->queue_index == (size_t)-1) {
        logkf("Error: scheduler null %d\n", get_current_cpuid());
        cpu_hlt;
    }
    apu_idle->parent_group = kernel_group;
    apu_idle->group_index  = queue_enqueue(kernel_group->pcb_queue, apu_idle);
    logkf("APU %d: %p %p started.\n", get_current_cpuid(), cpu->current_pcb, cpu->idle_pcb);
    cpu_done_count++;
    ticket_unlock(&apu_lock);
    open_interrupt;
    apu_hlt();
}

smp_cpu_t *get_cpu_smp(uint32_t processor_id) {
    smp_cpu_t *cpu = &cpus[processor_id];
    if (cpu->flags == 1) { return &cpus[processor_id]; }
    return NULL;
}

void apu_startup(struct limine_smp_request smp_request) {
    ticket_init(&apu_lock);
    struct limine_smp_response *response = smp_request.response;
    cpu_count                            = response->cpu_count;
    for (uint64_t i = 0; i <= cpu_count && i < MAX_CPU - 1; i++) {
        struct limine_smp_info *info   = response->cpus[i];
        size_t                  cpuid0 = info == NULL ? i : info->processor_id;
        cpus[cpuid0].scheduler_queue   = queue_init();
        cpus[cpuid0].iter_node         = NULL;
        cpus[cpuid0].lapic_id          = info == NULL ? i : info->lapic_id;
        cpus[cpuid0].directory         = get_kernel_pagedir();
        if (info == NULL) continue;
        if (info->lapic_id == response->bsp_lapic_id) {
            cpus[cpuid0].flags       = 1;
            bsp_processor_id         = info->processor_id;
            cpus[cpuid0].idle_pcb    = kernel_head_task;
            cpus[cpuid0].current_pcb = kernel_head_task;
        } else {
            info->goto_address = (void *)apu_entry;
        }
    }
    while (cpu_done_count < (cpu_count - 1) && cpu_done_count < (MAX_CPU - 1)) {
        __asm__ volatile("pause" ::: "memory");
    }
    logkf("APU %d processors have been enabled.\n", cpu_count);

    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    if (cpu == NULL) { return; }
    cpu->idle_pcb                 = kernel_head_task;
    kernel_head_task->queue_index = queue_enqueue(cpu->scheduler_queue, kernel_head_task);
    if (kernel_head_task->queue_index == (size_t)-1) {
        logkf("Error: scheduler null %d\n", get_current_cpuid());
    }

    kinfo("%d processors have been enabled.", cpu_count);
}
