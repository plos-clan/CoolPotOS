#include "lock.h"
#include "ptrace.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "term/klog.h"
#include "timer.h"
#include "hpet.h"

spin_t tsc_lock = SPIN_INIT;

void cpuid(uint32_t code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code) : "memory");
}

bool cpuid_has_sse() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return edx & (1 << 25);
}

bool cpu_has_rdtsc() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 4)) != 0;
}

uint64_t read_tsc() {
    //    uint32_t low, high;
    //    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    //    return ((uint64_t)high << 32) | low;
    __asm__ volatile("");
    return __builtin_ia32_rdtsc();
}

size_t sched_clock() {
    if (!arch_current_cpu()->arch_data.support_tsc) return nano_time();
    uint64_t now   = read_tsc();
    uint64_t delta = now - arch_current_cpu()->arch_data.tsc_base_tsc;
    uint64_t ns    = ((delta * (uint64_t)arch_current_cpu()->arch_data.tsc_conv_mul) >>
                   arch_current_cpu()->arch_data.tsc_conv_shift);
    return (ns - arch_current_cpu()->arch_data.tsc_base_tsc) / 1000;
}

void calibrate_tsc_with_hpet() {
    spin_lock(tsc_lock);
    bool is_bsp = arch_current_cpu()->id == get_bsp_cpu_id();
    arch_current_cpu()->arch_data.support_tsc = cpu_has_rdtsc();
    if (!arch_current_cpu()->arch_data.support_tsc) goto end;
    const uint64_t target_ns = 10 * 1000 * 1000;
    uint64_t       tsc_start = read_tsc();
    uint64_t       ns_start  = nano_time();
    nsleep(target_ns);
    uint64_t tsc_end   = read_tsc();
    uint64_t ns_end    = nano_time();
    uint64_t delta_tsc = tsc_end - tsc_start;
    uint64_t delta_ns  = ns_end - ns_start;
    if (delta_tsc == 0 || delta_ns == 0) return;
    arch_current_cpu()->arch_data.tsc_conv_shift = 22;
    arch_current_cpu()->arch_data.tsc_conv_mul =
        (uint32_t)((delta_ns << arch_current_cpu()->arch_data.tsc_conv_shift) / delta_tsc);
    arch_current_cpu()->arch_data.tsc_base_tsc = read_tsc();
    arch_current_cpu()->arch_data.tsc_base_ns  = nano_time();
    uint64_t freq_hz                           = (delta_tsc * 1000000000ull) / delta_ns;
    if(is_bsp) kinfo("Estimated TSC frequency: %llu MHz", freq_hz / 1000 / 1000);
end:
    if(is_bsp) kinfo("%s clock time %llu", cpu_has_rdtsc() ? "TSC" : "HPET", sched_clock());
    spin_unlock(tsc_lock);
}

void arch_task_scheduler(struct pt_regs *regs) {
    tcb_t thread = pick_next_task(arch_current_cpu()->id);
}
