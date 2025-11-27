#include "smp_rv64.h"
#include "atomic.h"
#include "krlibc.h"
#include "lib/libfdt/libfdt.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "rv64_irq.h"
#include "sbi.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "timer_rv64.h"

#define EARLY_MAP_BASE 0x80000000
#define EARLY_MAP_END  0x88000000

int                nr_cpu = 256;
uint64_t           cpuid_to_hartids[MAX_CPU];
atomic_t           started_cpu_count;
extern uint64_t    bsp_hart_id;
extern uintptr_t   opensbi_dtb_vaddr;
extern uintptr_t   smp_entry;
extern void        arch_cpu_init();
extern cpu_local_t cpu_local_infos[MAX_CPU];

extern pcb_t kernel_process;
extern tcb_t bsp_idle_thread;

uint64_t hartid_to_cpuid(uint64_t hartid) {
    for (size_t i = 0; i < MAX_CPU; ++i) {
        if (cpuid_to_hartids[i] == hartid) { return i; }
    }
    return -1;
}

uint64_t cpuid_to_hartid(uint64_t cpuid) {
    if (cpuid > MAX_CPU) return -1;
    return cpuid_to_hartids[cpuid];
}

_Noreturn void arch_ap_cpu_entry(uint64_t hartid) {
    __asm__ volatile("mv gp, %0" : : "r"(hartid));
    trap_init();
    arch_cpu_init();
    size_t cpuid                      = hartid_to_cpuid(hartid);
    cpu_local_infos[cpuid].enable     = true;
    cpu_local_infos[cpuid].task_count = 0;
    cpu_local_infos[cpuid].directory  = get_kernel_pagedir();
    cpu_local_infos[cpuid].id         = cpuid;
    __asm__ volatile("mv tp, %0\n\t" ::"r"(&cpu_local_infos[cpuid]));

    tcb_t idle_thread     = malloc(STACK_SIZE);
    idle_thread->process  = kernel_process;
    idle_thread->tid      = alloc_tid();
    idle_thread->ct_index = cow_list_add(kernel_process->child_threads, idle_thread);
    idle_thread->status   = T_RUNNING;
    set_cpu_idle_task(idle_thread, arch_current_cpu());
    arch_context_init(idle_thread,&idle_thread->context);

    timer_init_hart(hartid);
    atomic_inc(&started_cpu_count);
    arch_open_interrupt();
    while (true)
        arch_wait_for_interrupt();
}

cpu_local_t *arch_current_cpu() {
    cpu_local_t *cpu_local_info = NULL;
    __asm__ volatile("mv %0, tp" : "=r"(cpu_local_info));
    return cpu_local_info;
}

void arch_bsp_cpu_init() {
    __asm__ volatile("mv gp, %0" : : "r"(cpuid_to_hartids[0]));
    cpu_local_infos[0].enable       = true;
    __asm__ volatile("mv tp, %0\n\t" ::"r"(&cpu_local_infos[0]));
    set_bsp_cpu_info(arch_current_cpu());
    timer_init_hart(cpuid_to_hartid(0));
}

// start.S
extern void _opensbi_start();
extern void apu_start();

void smp_cpu_init(uint64_t *cpu_count0, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos) {
    uint64_t cpu_count            = 0;
    cpuid_to_hartids[cpu_count++] = bsp_hart_id;
    *bsp_cpu_id                   = 0;
    atomic_inc(&started_cpu_count);

    struct {
        uint64_t sp;
        uint64_t satp;
    } apu_arg[MAX_CPU];

    int offset = -1;
    while ((offset = fdt_next_node((void *)opensbi_dtb_vaddr, offset, NULL)) >= 0) {
        const char *name = fdt_get_name((void *)opensbi_dtb_vaddr, offset, NULL);
        if (!name) continue;
        if (strncmp(name, "cpu@", 4) == 0) {
            int            reg_len;
            const fdt32_t *reg =
                (const fdt32_t *)fdt_getprop((void *)opensbi_dtb_vaddr, offset, "reg", &reg_len);
            if (reg && reg_len >= (int)sizeof(uint32_t)) {
                uint32_t hartid = fdt32_to_cpu(reg[0]);
                if (hartid == bsp_hart_id) { continue; }
                uint64_t cpu_id          = cpu_count++;
                cpuid_to_hartids[cpu_id] = hartid;

                apu_arg[cpu_id].sp = (uint64_t)aligned_alloc(PAGE_SIZE, 32768);
                apu_arg[cpu_id].satp =
                    MAKE_SATP_PADDR(SATP_MODE_SV48, 0, virt_to_phys(get_kernel_pagedir()->table));
                uint64_t rv = sbi_ecall(0x48534D, 0, hartid, virt_to_phys(apu_start),
                          virt_to_phys(&apu_arg[cpu_id]), 0, 0, 0);
                (void)rv;
                continue;
            }
        }
    }

    while (atomic_read(&started_cpu_count) != cpu_count)
        arch_pause();

    *cpu_count0 = cpu_count;
}
