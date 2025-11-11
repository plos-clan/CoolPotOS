#include "smp_rv64.h"
#include "task/smp.h"
#include "krlibc.h"

int nr_cpu = 256;

_Noreturn void arch_ap_cpu_entry(){
    while (true) arch_wait_for_interrupt();
}

cpu_local_t *arch_current_cpu() {
    return NULL;
    //TODO
}

void arch_bsp_cpu_init() {

}

void smp_cpu_init(uint64_t *cpu_count, uint64_t *bsp_cpu_id, cpu_local_t *cpu_local_infos) {

}
