#include "smp_la64.h"
#include "task/smp.h"

int nr_cpu = 256;

cpu_local_t *arch_current_cpu() {
    return NULL;
}

void arch_bsp_cpu_init() {
}

void smp_cpu_init(uint64_t *cpu_count0, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos) {
}