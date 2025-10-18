#include "task/smp.h"
#include "krlibc.h"
#include "limine.h"
#include "term/klog.h"

LIMINE_REQUEST struct limine_smp_request mp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
#if defined(__x86_64__) || defined(__amd64__)
    .flags = LIMINE_SMP_X2APIC,
#endif
};

#if defined(__riscv__)
LIMINE_REQUEST struct limine_riscv_bsp_hartid_request bsp_hartid_request = {
    .id       = LIMINE_RISCV_BSP_HARTID_REQUEST,
    .revision = 0,
};
#endif

#if defined(__x86_64__) || defined(__amd64__)
bool x2apic_mode_supported() {
    return !!(mp_request.response->flags & LIMINE_SMP_X2APIC);
}
#endif

uint64_t    cpu_count  = 0;
uint64_t    bsp_cpu_id = 0;
cpu_local_t cpu_local_infos[MAX_CPU];

cpu_local_t *get_cpu_local(size_t id) {
    if (id >= MAX_CPU) return NULL;
    if (!cpu_local_infos[id].enable) return NULL;
    return &cpu_local_infos[id];
}

void smp_init() {
    struct limine_smp_response *mp_response = mp_request.response;
    cpu_count                               = mp_response->cpu_count;

    for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
        struct limine_smp_info *cpu = mp_response->cpus[i];
#if defined(__x86_64__) || defined(__amd64__)
        cpu_local_infos[i].id = cpu->lapic_id;
        bsp_cpu_id            = mp_response->bsp_lapic_id;
        if (cpu->lapic_id == mp_response->bsp_lapic_id) continue;
#endif
#if defined(__aarch64__)
        cpu_local_infos[i].id = cpu->mpidr;
        if (cpu->mpidr == mp_request.response->bsp_mpidr) continue;
        bsp_cpu_id = mp_request.response->bsp_mpidr;
#endif
#if defined(__riscv__)
        cpu_local_infos[i].id = cpu->hartid;
        if (cpu->hartid == bsp_hartid_request.response->bsp_hartid) continue;
        bsp_cpu_id = bsp_hartid_request.response->bsp_hartid;
#endif
        cpu_local_infos[i].enable = true;
        cpu->goto_address         = (limine_goto_address)arch_ap_cpu_entry;
    }

    kinfo("%d processors have been enabled.", cpu_count);
}
