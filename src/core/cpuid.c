#include "cpuid.h"
#include "klog.h"
#include "krlibc.h"

cpu_t cpu;

static void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx) {
    *eax = op;
    *ecx = 0;
    __asm__ volatile("cpuid"
            : "=a" (*eax),                //输出参数
    "=b" (*ebx),
    "=c" (*ecx),
    "=d" (*edx)
            : "0" (*eax), "2" (*ecx)    //输入参数
            : "memory");
}

static void get_vendor_name(cpu_t *c) {
    int cpuid_level;
    static char x86_vendor_id[16] = {0};
    cpuid(0x00000000, (unsigned int *) &cpuid_level,
          (unsigned int *) &x86_vendor_id[0],
          (unsigned int *) &x86_vendor_id[8],
          (unsigned int *) &x86_vendor_id[4]);
    c->vendor = x86_vendor_id;
}

static void get_model_name(cpu_t *c) {
    unsigned int *v = (unsigned int *) c->model_name;
    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
    c->model_name[48] = 0;
}

static void get_cpu_address_sizes(cpu_t *c) {
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

    c->virt_bits = (eax >> 8) & 0xff;
    c->phys_bits = eax & 0xff;
}

void init_cpuid(){
    memset(cpu.vendor, 0, sizeof(cpu.vendor));
    get_vendor_name(&cpu);
    get_model_name(&cpu);
    get_cpu_address_sizes(&cpu);
    printk("CPUID: %s %s | phy/virt: %d/%d bits\n",cpu.vendor,cpu.model_name,cpu.phys_bits,cpu.virt_bits);
}

cpu_t *get_cpuid(){
    return &cpu;
}