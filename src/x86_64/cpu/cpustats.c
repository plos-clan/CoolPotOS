#include "cpustats.h"
#include "io.h"
#include "timer.h"

uint64_t read_tsc() {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

uint64_t read_mpc() {
    return rdmsr(IA32_MPERF);
}

uint64_t read_apfcc() {
    return rdmsr(IA32_APERF);
}

uint64_t get_cpu_base_freq_hz() {
    uint64_t val   = rdmsr(IA32_PLATFORM_INFO);
    uint64_t ratio = (val >> 8) & 0xFF; // Bits 15:8
    return ratio * 100000000ULL;        // 100 MHz × ratio → Hz
}

uint64_t estimate_actual_freq() {
    uint64_t base_freq_hz = get_cpu_base_freq_hz();
    uint64_t aperf0       = read_apfcc();
    uint64_t mperf0       = read_mpc();

    nsleep(10000000UL);

    uint64_t aperf1 = read_apfcc();
    uint64_t mperf1 = read_mpc();

    uint64_t d_aperf = aperf1 - aperf0;
    uint64_t d_mperf = mperf1 - mperf0;

    if (d_mperf == 0) return 0;
    return (d_aperf * base_freq_hz) / d_mperf;
}
