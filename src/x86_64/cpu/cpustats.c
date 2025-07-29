#include "cpustats.h"
#include "cpuid.h"
#include "klog.h"
#include "kprint.h"
#include "timer.h"

uint32_t tsc_conv_mul;
uint32_t tsc_conv_shift;
uint64_t tsc_base_tsc;
uint64_t tsc_base_ns;

uint64_t read_tsc() {
    //    uint32_t low, high;
    //    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    //    return ((uint64_t)high << 32) | low;
    __asm__ volatile("");
    return __builtin_ia32_rdtsc();
}

size_t sched_clock() {
    if (!cpu_has_rdtsc()) return nano_time();
    uint64_t now   = read_tsc();
    uint64_t delta = now - tsc_base_tsc;
    uint64_t ns    = ((delta * (uint64_t)tsc_conv_mul) >> tsc_conv_shift);
    return tsc_base_ns + ns;
}

void calibrate_tsc_with_hpet() {
    if (!cpu_has_rdtsc()) goto end;
    const uint64_t target_ns = 10 * 1000 * 1000;
    uint64_t       tsc_start = read_tsc();
    uint64_t       ns_start  = nano_time();
    nsleep(target_ns);
    uint64_t tsc_end   = read_tsc();
    uint64_t ns_end    = nano_time();
    uint64_t delta_tsc = tsc_end - tsc_start;
    uint64_t delta_ns  = ns_end - ns_start;
    if (delta_tsc == 0 || delta_ns == 0) return;
    tsc_conv_shift   = 22;
    tsc_conv_mul     = (uint32_t)((delta_ns << tsc_conv_shift) / delta_tsc);
    tsc_base_tsc     = read_tsc();
    tsc_base_ns      = nano_time();
    uint64_t freq_hz = (delta_tsc * 1000000000ull) / delta_ns;
    kinfo("Estimated TSC frequency: %llu MHz", freq_hz / 1000 / 1000);
end:
    kinfo("%s clock time %llu", cpu_has_rdtsc() ? "TSC" : "HPET", sched_clock());
}
