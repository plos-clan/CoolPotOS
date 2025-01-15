#include "cpuid.h"
#include "krlibc.h"
#include "kprint.h"

cpu_t cpu;

void cpuid(uint32_t code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid"
            : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
            : "a"(code)
            : "memory");
}

static void get_vendor_name() {
    int cpuid_level;
    static char x86_vendor_id[16] = {0};
    cpuid(0x00000000, (unsigned int *) &cpuid_level,
          (unsigned int *) &x86_vendor_id[0],
          (unsigned int *) &x86_vendor_id[8],
          (unsigned int *) &x86_vendor_id[4]);
    cpu.vendor = x86_vendor_id;
}

static void get_model_name() {
    unsigned int *v = (unsigned int *) cpu.model_name;
    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
    cpu.model_name[48] = 0;
}

static void get_cpu_address_sizes() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

    cpu.virt_bits = (eax >> 8) & 0xff;
    cpu.phys_bits = eax & 0xff;
}

cpu_t get_cpu_info() {
    return cpu;
}

void init_cpuid() {
    get_vendor_name();
    get_model_name();
    get_cpu_address_sizes();
    printk("CPU: %s %s | phy/virt: %d/%d bits\n", cpu.vendor, cpu.model_name, cpu.phys_bits, cpu.virt_bits);
}
