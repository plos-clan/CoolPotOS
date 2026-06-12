#include "cpu/fsgsbase.h"

#include "io.h"

uint64_t read_fsbase_msr() {
    return rdmsr(IA32_FS_BASE);
}

void write_fsbase_msr(uint64_t value) {
    wrmsr(IA32_FS_BASE, value);
}

uint64_t read_gsbase_msr() {
    return rdmsr(IA32_GS_BASE);
}

void write_gsbase_msr(uint64_t value) {
    wrmsr(IA32_GS_BASE, value);
}

static uint64_t (*ptr_read_fsbase)()            = read_fsbase_msr;
static void (*ptr_write_fsbase)(uint64_t value) = write_fsbase_msr;
static uint64_t (*ptr_read_gsbase)()            = read_gsbase_msr;
static void (*ptr_write_gsbase)(uint64_t value) = write_gsbase_msr;

uint64_t rdfsbase() {
    uint64_t ret;
    __asm__ volatile("rdfsbase %0" : "=r"(ret));
    return ret;
}

void wrfsbase(uint64_t value) {
    __asm__ volatile("wrfsbase %0" ::"r"(value));
}

uint64_t rdgsbase() {
    uint64_t ret;
    __asm__ volatile("rdgsbase %0" : "=r"(ret));
    return ret;
}

void wrgsbase(uint64_t value) {
    __asm__ volatile("wrgsbase %0" ::"r"(value));
}

uint64_t read_kgsbase() {
    return rdmsr(IA32_KERNEL_GS_BASE);
}

void write_kgsbase(uint64_t value) {
    wrmsr(IA32_KERNEL_GS_BASE, value);
}

uint64_t read_fsbase() {
    return ptr_read_fsbase();
}

void write_fsbase(uint64_t value) {
    ptr_write_fsbase(value);
}

uint64_t read_gsbase() {
    return ptr_read_gsbase();
}

void write_gsbase(uint64_t value) {
    ptr_write_gsbase(value);
}

static uint32_t has_fsgsbase() {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x07), "c"(0x00));
    return ebx & (1 << 0);
}

void fsgsbase_init() {
    uint32_t support = has_fsgsbase();
    if (support) {
        uint64_t cr4 = 0;
        __asm__ volatile("movq %%cr4, %0" : "=r"(cr4));
        cr4 |= 1 << 16;
        __asm__ volatile("movq %0, %%cr4" ::"r"(cr4));

        ptr_read_fsbase  = rdfsbase;
        ptr_write_fsbase = wrfsbase;
        ptr_read_gsbase  = rdgsbase;
        ptr_write_gsbase = wrgsbase;
    }
}
