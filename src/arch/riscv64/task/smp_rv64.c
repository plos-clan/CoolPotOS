#include "smp_rv64.h"
#include "atomic.h"
#include "krlibc.h"
#include "lib/libfdt/libfdt.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "rv64_irq.h"
#include "sbi.h"
#include "task/smp.h"

#define EARLY_MAP_BASE 0x80000000
#define EARLY_MAP_END  0x88000000

int              nr_cpu = 256;
uint64_t         cpuid_to_hartid[MAX_CPU];
bool             cpu_done_status[MAX_CPU];
atomic_t         started_cpu_count;
extern uint64_t  bsp_hart_id;
extern uintptr_t opensbi_dtb_vaddr;
extern uintptr_t smp_entry;

_Noreturn void arch_ap_cpu_entry(uint64_t hartid) {
    __asm__ volatile("mv gp, %0" : : "r"(hartid));
    switch_page_directory(get_kernel_pagedir());
    trap_init();
    atomic_inc(&started_cpu_count);
    while (true)
        arch_wait_for_interrupt();
}

cpu_local_t *arch_current_cpu() {
    return NULL;
    //TODO
}

void arch_bsp_cpu_init() {}

extern void _opensbi_start(); // start.S

void smp_cpu_init(uint64_t *cpu_count0, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos) {
    uint64_t cpu_count           = 0;
    cpuid_to_hartid[cpu_count++] = bsp_hart_id;
    *bsp_cpu_id                  = 0;
    atomic_inc(&started_cpu_count);

    page_map_range(get_kernel_pagedir(), EARLY_MAP_BASE, EARLY_MAP_BASE,
                   EARLY_MAP_END - EARLY_MAP_BASE, KERNEL_PTE_FLAGS | ARCH_PT_FLAG_EXEC);
    smp_entry = (uintptr_t)arch_ap_cpu_entry;

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
                cpuid_to_hartid[cpu_count++] = hartid;
                sbi_ecall(0x48534D, 0, hartid, 0x80200000,0, 0, 0, 0);
                while (atomic_read(&started_cpu_count) != cpu_count)
                    arch_pause();
            }
        }
    }

end:
    unmap_page_range(get_kernel_pagedir(), EARLY_MAP_BASE, EARLY_MAP_END - EARLY_MAP_BASE);
    *cpu_count0 = cpu_count;
}
