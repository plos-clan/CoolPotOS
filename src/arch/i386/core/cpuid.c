#include "cpuid.h"
#include "klog.h"
#include "krlibc.h"

cpu_t cpu;

// 获取CPUID信息并填充结构体cpu_t的相关字段
static void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx,
                  unsigned int *edx) {
    // 设置初始值
    *eax = op; // 将操作码赋值给EAX寄存器
    *ecx = 0;  // 清空ECX寄存器

    __asm__ volatile("cpuid"
                     : "=a"(*eax), // 输出参数 - EAX寄存器的结果
                       "=b"(*ebx), // 输出参数 - EBX寄存器的结果
                       "=c"(*ecx), // 输出参数 - ECX寄存器的结果
                       "=d"(*edx)  // 输出参数 - EDX寄存器的结果
                     : "0"(*eax),  // 输入参数 - EAX初始值（操作码op）
                       "2"(*ecx)   // 输入参数 - ECX的值（通常用于指定leaf）
                     : "memory");  // 声明内存可能被修改，避免编译器优化
}

// 获取CPU厂商名称并填充到cpu_t结构体中
static void get_vendor_name(cpu_t *c) {
    int         cpuid_level;             // 用于接收最高支持的CPUID级别
    static char x86_vendor_id[16] = {0}; // 存储12个字符的厂商信息

    // 调用CPUID指令，获取厂商ID信息（level 0x00000000）
    cpuid(0x00000000, (unsigned int *)&cpuid_level,
          (unsigned int *)&x86_vendor_id[0],  // EAX部分
          (unsigned int *)&x86_vendor_id[8],  // EBX部分
          (unsigned int *)&x86_vendor_id[4]); // ECX部分

    // 将厂商ID字符串指针赋值给cpu_t结构体的vendor字段
    c->vendor = x86_vendor_id;
}

// 获取CPU型号名称并填充到cpu_t结构体中
static void get_model_name(cpu_t *c) {
    unsigned int *v = (unsigned int *)c->model_name; // 获取模型名数组的指针

    // 按照Intel推荐的方法获取扩展的处理器信息（三个不同的leaf）
    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);   // 第一个部分
    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);   // 第二个部分
    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]); // 第三个部分

    // 确保字符串终止
    c->model_name[48] = 0;
}

// 获取CPU的虚拟地址和物理地址位数信息
static void get_cpu_address_sizes(cpu_t *c) {
    unsigned int eax, ebx, ecx, edx; // 用于接收CPUID返回值

    // 调用CPUID获取地址转换和物理地址长度（level 0x80000008）
    cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

    // 解析EAX寄存器的高8位和低8位
    c->virt_bits = (eax >> 8) & 0xff; // 获取虚拟地址位数
    c->phys_bits = eax & 0xff;        // 获取物理地址位数
}

// 初始化CPU信息并打印相关信息
void init_cpuid() {
    // 首先清空厂商字段
    memset(cpu.vendor, 0, sizeof(cpu.vendor));

    // 获取各种CPU信息
    get_vendor_name(&cpu);       // 厂商名称
    get_model_name(&cpu);        // 型号名称
    get_cpu_address_sizes(&cpu); // 地址位数

    // 打印获取到的CPU信息
    printk("CPUID: %s %s | phy/virt: %d/%d bits\n",
           cpu.vendor,     // 厂商
           cpu.model_name, // 型号名称
           cpu.phys_bits,  // 物理地址位数
           cpu.virt_bits); // 虚拟地址位数
}

// 获取cpu_t结构体的指针
cpu_t *get_cpuid() {
    return &cpu;
}
