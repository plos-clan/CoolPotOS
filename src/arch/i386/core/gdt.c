#include "description_table.h"
#include "krlibc.h"

// GDT表项结构体数组，用于存储全局描述表中的各个段或任务描述符
gdt_entry_t gdt_entries[GDT_LENGTH];
// GDT指针结构体，保存GDT的起始地址和长度
gdt_ptr_t   gdt_ptr;

// TSS结构体实例，用于存储当前处理器上下文信息，如栈指针、段寄存器等
tss_entry tss;

/**
 * 写入TSS到指定的GDT位置，并进行初始化配置。
 *
 * @param num GDT中描述符的索引位置
 * @param ss0 TSS的第一个段选择子（通常用于栈段）
 * @param esp0 内核模式下的初始堆栈指针
 */
void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
    // 计算TSS在内存中的起始地址和长度
    uintptr_t base  = (uintptr_t)&tss;
    uintptr_t limit = base + sizeof(tss);

    // 在GDT的指定位置设置一个可调用门（TSS）
    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    // 将TSS结构体所有字段初始化为0
    memset(&tss, 0x0, sizeof(tss));

    // 设置TSS的段寄存器值和初始栈指针
    tss.ss0  = ss0;  // 栈段选择子
    tss.esp0 = esp0; // 内核模式下的栈顶指针

    // 设置代码段、数据段选择子（此处均为特定值，需根据实际需求调整）
    tss.cs = 0x0b; // 代码段选择子
    tss.ss = 0x13; // 堆栈段选择子
    tss.ds = 0x13; // 数据段选择子
    tss.es = 0x13; // 额外段选择子
    tss.fs = 0x13; // F段选择子
    tss.gs = 0x13; // G段选择子

    // 设置I/O映射基地址为TSS的大小（用于保护I/O操作）
    tss.iomap_base = sizeof(tss);
}

/**
 * 在GDT指定位置设置一个新的段或任务描述符。
 *
 * @param num GDT中的索引位置
 * @param base 段的基地址
 * @param limit 段的长度
 * @param access 访问权限标志（决定可读性、可写性等）
 * @param gran 粒度控制，指定段尺寸单位是字还是4KB页
 */
void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // 将基地址分解为低、中、高三个部分分别存储
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    // 将界限分解为低16位和高4位，并结合粒度控制
    gdt_entries[num].limit_low    = (limit & 0xFFFF);
    gdt_entries[num].granularity  = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0; // 组合粒度标志

    // 设置段的访问权限
    gdt_entries[num].access = access;
}

/**
 * 设置内核模式下的栈指针到TSS中。
 *
 * @param stack 新的堆栈地址，用于在特权级切换时使用
 */
void set_kernel_stack(uintptr_t stack) {
    // 更新TSS中的esp0字段，即内核模式下的默认栈顶指针
    tss.esp0 = stack;
}

/**
 * 安装并初始化GDT，配置各个段描述符，并加载到处理器中。
 */
void gdt_install() {
    // 设置GDT的基地址和长度
    gdt_ptr.limit = sizeof(gdt_entry_t) * GDT_LENGTH - 1; // GDT总字节数减一作为段界限
    gdt_ptr.base  = (uint32_t)&gdt_entries;               // GDT起始地址

    // 按照Intel文档要求，第一个描述符必须全零
    gdt_set_gate(0, 0, 0, 0, 0);

    // 设置内核模式下的代码段和数据段描述符
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 0x9A：可执行，环级别0；0xCF：4KB粒度，可32位访问
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 0x92：可读写，环级别0；0xCF：4KB粒度，可32位访问

    // 设置用户模式下的代码段和数据段描述符
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 0xFA：可执行，环级别3；0xCF：4KB粒度，可32位访问
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 0xF2：可读写，环级别3；0xCF：4KB粒度，可32位访问

    // 刷新GDT，使其生效。参数为GDT指针的地址
    gdt_flush((uint32_t)&gdt_ptr);

    // 获取当前的堆栈指针，并将其设置到TSS中作为默认内核栈指针
    uint32_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    write_tss(5, 0x10, esp); // 假设在GDT的第五个位置存放TSS，段选择子为0x10

    // 刷新当前任务状态段，以确保处理器使用新的TSS信息
    tss_flush();
}
