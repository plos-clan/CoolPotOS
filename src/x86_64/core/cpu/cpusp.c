#include "cpusp.h"
#include "kprint.h"
#include "timer.h"

uint64_t ips = 0;

uint64_t get_cpu_speed(){
    return ips;
}

_Noreturn void cpu_speed_test(){
    infinite_loop{
        size_t _start_t = nanoTime();
        for (volatile uint64_t i = 0; i < 1000; i++);
        size_t _end_t = nanoTime() - _start_t;
        ips = _end_t / 1000;
    }
}
