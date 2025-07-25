#include "cpustats.h"
#include "io.h"
#include "timer.h"

uint64_t read_tsc() {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}
