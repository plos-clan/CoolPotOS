#include "cpu_local.h"
#include "smp_x64.h"
#include "cpu/fsgsbase.h"
#include "krlibc.h"

static cpu_local_t local_infos[MAX_CPU_NUM];
static x86_64_local_info_t arch_local_infos[MAX_CPU_NUM];

cpu_local_t *get_current_cpu() {
    return (cpu_local_t *)read_kgsbase();
}

void x64_cpu_local_init(size_t cpu_id, size_t lapic_id) {
    if (cpu_id >= MAX_CPU_NUM) {
        return;
    }
    cpu_local_t *local              = &local_infos[cpu_id];
    x86_64_local_info_t *arch_local = &arch_local_infos[cpu_id];
    memset(local, 0, sizeof(*local));
    memset(arch_local, 0, sizeof(*arch_local));
    local->cpu_id          = cpu_id;
    local->arch_local_info = arch_local;

    arch_local->lapic_id = lapic_id;

    write_kgsbase((uint64_t)local);
}

void arch_bsp_cpu_setup() {
}
