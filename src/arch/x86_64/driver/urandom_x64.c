#include "cpu_features_x64.h"
#include "driver/urandom.h"
#include "krlibc.h"
#include "task/smp.h"

static uint64_t rdtsc_read(void) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
}

static bool rdrand64(uint64_t *val) {
    unsigned char ok = 0;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok != 0;
}

static bool rdseed64(uint64_t *val) {
    unsigned char ok = 0;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok != 0;
}

static uint64_t prng_state = 0;

static uint64_t prng_next(void) {
    if (prng_state == 0) {
        uint64_t seed    = rdtsc_read() ^ (uintptr_t)&prng_state;
        cpu_local_t *cpu = arch_current_cpu();
        if (cpu) {
            seed ^= (uint64_t)cpu->id << 32;
        }
        if (seed == 0) {
            seed = 0x9e3779b97f4a7c15ULL;
        }
        prng_state = seed;
    }

    uint64_t x = prng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    prng_state = x;
    return x * 2685821657736338717ULL;
}

bool arch_get_random_bytes(uint8_t *buf, size_t size) {
    if (!buf) {
        return false;
    }

    const bool have_rdseed = has_cpu_features_ebx(CPUID_EBX_RDSEED);
    const bool have_rdrand = has_cpu_features_ecx(CPUID_ECX_RDRAND);

    while (size > 0) {
        uint64_t val = 0;
        bool ok      = false;

        if (have_rdseed) {
            for (int i = 0; i < 8 && !ok; i++) {
                ok = rdseed64(&val);
            }
        }
        if (!ok && have_rdrand) {
            for (int i = 0; i < 8 && !ok; i++) {
                ok = rdrand64(&val);
            }
        }
        if (!ok) {
            val = prng_next();
        }

        const size_t chunk = size > sizeof(val) ? sizeof(val) : size;
        memcpy(buf, &val, chunk);
        buf += chunk;
        size -= chunk;
    }

    return true;
}
