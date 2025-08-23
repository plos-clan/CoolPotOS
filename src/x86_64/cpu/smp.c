#include "smp.h"
#include "boot.h"
#include "description_table.h"
#include "eevdf.h"
#include "fpu.h"
#include "fsgsbase.h"
#include "heap.h"
#include "io.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pcb.h"
#include "sprintf.h"
#include "syscall.h"

extern struct idt_register idt_pointer;      // idt.c
extern tcb_t               kernel_head_task; // scheduler.c
extern bool                x2apic_mode;      // apic.c
extern pcb_t               kernel_group;     // pcb.c
extern int                 now_tid;          // pcb.c
spin_t                     apu_lock;

smp_cpu_t smp_cpus[MAX_CPU];
uint32_t  bsp_processor_id;
uint64_t  cpu_count = 0;

static volatile uint64_t cpu_done_count = 0;

uint64_t cpu_num() {
    return cpu_count;
}

uint32_t get_current_cpuid() {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x0B), "c"(0));
    return edx;
}

static __attr(naked) void _setcs_helper() {
    __asm__ volatile("pop %%rax\n\t"
                     "push %%rbx\n\t"
                     "push %%rax\n\t"
                     "lretq\n\t" ::
                         : "memory");
}

static void apu_gdt_setup() {
    uint32_t   this_id  = lapic_id(); //get_current_cpuid();
    smp_cpu_t *this_cpu = &smp_cpus[this_id];

    this_cpu->gdtEntries[0] = 0x0000000000000000U;
    this_cpu->gdtEntries[1] = 0x00a09a0000000000U;
    this_cpu->gdtEntries[2] = 0x00c0920000000000U;
    this_cpu->gdtEntries[3] = 0x00c0f20000000000U;
    this_cpu->gdtEntries[4] = 0x00a0fa0000000000U;

    this_cpu->gdt_pointer = ((struct gdt_register){
        .size = ((uint16_t)((uint32_t)sizeof(gdt_entries_t) - 1U)),
        .ptr  = &this_cpu->gdtEntries,
    });

    __asm__ volatile("lgdt %[ptr]\n\t"
                     "call *%%rax\n\t"
                     "mov %[dseg], %%ds\n\t"
                     "mov %[dseg], %%fs\n\t"
                     "mov %[dseg], %%gs\n\t"
                     "mov %[dseg], %%es\n\t"
                     "mov %[dseg], %%ss\n\t"
                     :
                     : [ptr] "m"(this_cpu->gdt_pointer), [dseg] "rm"((uint16_t)0x10U),
                       "a"(&_setcs_helper), "b"((uint16_t)0x8U)
                     : "memory");

    write_fsbase(0);
    write_gsbase((uint64_t)this_cpu);
    write_kgsbase((uint64_t)this_cpu);
    this_cpu->id = this_id;

    uint64_t address     = (uint64_t)&(this_cpu->tss0);
    uint64_t low_base    = (((address & 0xffffffU)) << 16U);
    uint64_t mid_base    = (((((address >> 24U)) & 0xffU)) << 56U);
    uint64_t high_base   = (address >> 32U);
    uint64_t access_byte = (((uint64_t)(0x89U)) << 40U);
    uint64_t limit       = ((uint64_t)(uint32_t)(sizeof(tss_t) - 1U));

    printk("", address); // 用于对抗编译器优化

    this_cpu->gdtEntries[5] = (((low_base | mid_base) | limit) | access_byte);
    this_cpu->gdtEntries[6] = high_base;

    this_cpu->tss0.ist[0] = ((uint64_t)&(this_cpu->tss_stack)) + sizeof(tss_stack_t);

    __asm__ volatile("ltr %[offset]\n\t" : : [offset] "rm"(0x28U) : "memory");
}

extern void halt_service();

_Noreturn void apu_entry() {
    spin_lock(apu_lock);
    extern uint64_t double_fault_page;
    __asm__ volatile("mov %0, %%cr3" : : "r"(double_fault_page));
    apu_gdt_setup();
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
    //local_apic_init(false);
    ap_local_apic_init();
    float_processor_setup();

    tcb_t apu_idle = (tcb_t)malloc(STACK_SIZE);
    not_null_assets(apu_idle, "apu idle null");
    apu_idle->task_level = TASK_IDLE_LEVEL;
    apu_idle->tid        = now_tid++;
    apu_idle->cpu_clock  = 0;
    set_kernel_stack(get_rsp());
    apu_idle->kernel_stack = apu_idle->context0.rsp = get_rsp();
    apu_idle->user_stack                            = apu_idle->kernel_stack;
    apu_idle->context0.rflags                       = get_rflags() | 0x200;
    apu_idle->cpu_timer                             = nano_time();
    apu_idle->cpu_id                                = current_cpu->id;
    apu_idle->status                                = RUNNING;
    char name[50];
    sprintf(name, "CP_IDLE_CPU%u", current_cpu->id);
    memcpy(apu_idle->name, name, strlen(name));
    apu_idle->name[strlen(name)] = '\0';
    current_cpu->idle_pcb        = apu_idle;
    current_cpu->ready           = true;
    change_current_tcb(apu_idle);
    apu_idle->parent_group = kernel_group;
    apu_idle->group_index  = queue_enqueue(kernel_group->pcb_queue, apu_idle);
    apu_idle->queue_index = queue_enqueue(((smp_cpu_t *)read_kgsbase())->scheduler_queue, apu_idle);
    if (apu_idle->queue_index == (size_t)-1) {
        kerror("Unable to add task to scheduler queue for CPU%d", current_cpu->id);
        current_cpu->ready = false;
        kerror("Scheduler queue %p", current_cpu->scheduler_queue);
        cpu_done_count++;
        spin_unlock(apu_lock);
        open_interrupt;
        cpu_hlt;
    }
    cpu_done_count++;
    spin_unlock(apu_lock);
    init_ap_idle(current_cpu, apu_idle);

    setup_syscall(false);

    create_kernel_thread((void *)halt_service, NULL, "free service", kernel_group, lapic_id());

    open_interrupt;
    loop __asm__ volatile("hlt");
}

smp_cpu_t *get_cpu_smp(uint32_t processor_id) {
    smp_cpu_t *cpu = &smp_cpus[processor_id];
    return cpu->ready ? cpu : NULL;
}

void smp_setup() {
    apu_lock                             = SPIN_INIT;
    struct limine_smp_response *response = get_smp_info();
    cpu_count                            = response->cpu_count;
    for (uint64_t i = 0; i < cpu_count && i < MAX_CPU - 1; i++) {
        struct limine_smp_info *info     = response->cpus[i];
        size_t                  cpuid0   = info->lapic_id;
        smp_cpus[cpuid0].scheduler_queue = queue_init();
        smp_cpus[cpuid0].death_queue     = queue_init();
        smp_cpus[cpuid0].lapic_id        = info->lapic_id;
        smp_cpus[cpuid0].directory       = get_kernel_pagedir();
        if (info->lapic_id == response->bsp_lapic_id) {
            bsp_processor_id      = info->lapic_id;
            current_cpu->ready    = true;
            current_cpu->idle_pcb = kernel_head_task;
            change_current_tcb(kernel_head_task);
        } else {
            info->goto_address = (void *)&apu_entry;
        }
    }
    while (cpu_done_count < (cpu_count - 1) && cpu_done_count < (MAX_CPU - 1)) {
        __asm__ volatile("pause" ::: "memory");
    }

    current_cpu->idle_pcb = kernel_head_task;
    kernel_head_task->queue_index =
        queue_enqueue(((smp_cpu_t *)read_kgsbase())->scheduler_queue, kernel_head_task);
    if (kernel_head_task->queue_index == (size_t)-1) {
        logkf("Error: scheduler null %d\n", current_cpu->id);
        kerror("Unable to add kernel head task to scheduler queue for CPU%d", current_cpu->id);
        current_cpu->ready = false;
    }

    close_interrupt;
    current_cpu->sched_handle = (struct eevdf_t *)calloc(1, sizeof(struct eevdf_t));
    ((struct eevdf_t *)current_cpu->sched_handle)->root =
        (struct rb_root *)malloc(sizeof(struct rb_root));
    ((struct eevdf_t *)current_cpu->sched_handle)->min_vruntime = 0;
    ((struct eevdf_t *)current_cpu->sched_handle)->wait_queue   = queue_init();
    ((struct eevdf_t *)current_cpu->sched_handle)->task_count   = 0;
    struct sched_entity *idle_entity = new_entity(kernel_head_task, NICE_TO_PRIO(0), current_cpu);
    ((struct eevdf_t *)current_cpu->sched_handle)->idle_entity = idle_entity;
    kernel_head_task->sched_handle                             = idle_entity;
    insert_sched_entity(((struct eevdf_t *)current_cpu->sched_handle)->root, idle_entity);
    eevdf_sched->current = idle_entity;
    open_interrupt;

    create_kernel_thread((void *)halt_service, NULL, "free_service", kernel_group, lapic_id());
    kinfo("%d processors have been enabled.", cpu_count);
}
