#include "description_table.h"
#include "io.h"
#include "krlibc.h"
// 中断服务例程处理数组，存储256种中断的处理函数，每个ISR对应一个处理器

static isr_t interrupt_handlers[256];

// IDT描述符表，共有256个条目，每个条目的结构由idt_entry_t定义
idt_entry_t idt_entries[256];

// IDT指针，用于加载IDT到CPU，包含了IDT的起始地址和大小限制
idt_ptr_t idt_ptr;

/**
 * @brief 初始化中断门，用指定的基地址填充IDT描述符表中的一个条目。
 *
 * @param num 中断向量号（0-255）
 * @param base ISR处理函数的基地址，会被分成高16位和低16位存储
 */
void idt_use_reg(uint8_t num,uint32_t base){
    // 将基地址拆分为低位和高位部分，并分别填充到IDT条目中
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;

    // 填充段选择子（CS），这里设定为0x08，即内核代码段的选择子
    idt_entries[num].sel = 0x08;
    // 保留位，必须为0
    idt_entries[num].always0 = 0;
    // 设置门的属性：0x8E表示可被Ring3调用，0x60是特权级别
    idt_entries[num].flags = 0x8E | 0x60;
}

/**
 * @brief 设置中断门，用指定的基地址、段选择子和标志填充IDT描述符表中的一个条目。
 *
 * @param num 中断向量号（0-255）
 * @param base ISR处理函数的基地址，会被分成高16位和低16位存储
 * @param sel 段选择子
 * @param flags 门的属性标志
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    // 将基地址拆分为低位和高位部分，并分别填充到IDT条目中
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;

    // 填充段选择子（CS）
    idt_entries[num].sel = sel;
    // 保留位，必须为0
    idt_entries[num].always0 = 0;
    // 设置门的属性
    idt_entries[num].flags = flags;
}

/**
 * @brief ISR中断处理函数，负责根据中断号调用注册的ISR处理程序。
 *
 * @param regs 包含中断发生时的寄存器状态的结构体
 */
void isr_handler(registers_t regs) {
    // 如果对应的中断号存在注册的处理函数，则调用该处理函数
    if (interrupt_handlers[regs.int_no]) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(&regs);
    }
}

/**
 * @brief IRQ中断处理函数，负责根据中断号调用注册的IRQ处理程序，并发送EOI。
 *
 * @param regs 包含中断发生时的寄存器状态的结构体
 */
void irq_handler(registers_t regs) {
    // 如果是0x2b中断则不需要发送EOI，其余情况下发送EOI给主从PIC
    if(regs.int_no != 0x2b) {
        // 如果中断号>=40，则向从片PIC端口发送EOI命令
        if (regs.int_no >= 40)
            outb(0xA0, 0x20);
        // 向主片PIC端口发送EOI命令
        outb(0x20, 0x20);
    }
    // 调用注册的中断处理程序
    if (interrupt_handlers[regs.int_no]) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(&regs);
    }
}

/**
 * @brief 注册ISR中断处理函数，将指定的中断号对应的处理函数存储在数组中。
 *
 * @param n 中断向量号（0-255）
 * @param handler 指向ISR处理函数的指针
 */
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

/**
 * @brief 安装IDT，初始化并加载IDT到CPU。
 */
void idt_install() {
    // 设置IDT的大小限制（255个条目）
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    // 设置IDT的基地址
    idt_ptr.base = (uint32_t) & idt_entries;

    // 将IDT的所有条目初始化为0
    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

    // 配置主片PIC的ICW1到ICW4，完成初始化
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

#define REGISTER_ISR(id) idt_set_gate(id, (uint32_t)isr##id, 0x08, 0x8E)
    // 为ISR向量0到30注册处理函数
    REGISTER_ISR(0);
    REGISTER_ISR(1);
    REGISTER_ISR(2);
    REGISTER_ISR(3);
    REGISTER_ISR(4);
    REGISTER_ISR(5);
    REGISTER_ISR(6);
    REGISTER_ISR(7);
    REGISTER_ISR(8);
    REGISTER_ISR(9);
    REGISTER_ISR(10);
    REGISTER_ISR(11);
    REGISTER_ISR(12);
    REGISTER_ISR(13);
    REGISTER_ISR(14);
    REGISTER_ISR(15);
    REGISTER_ISR(16);
    REGISTER_ISR(17);
    REGISTER_ISR(18);
    REGISTER_ISR(19);
    REGISTER_ISR(20);
    REGISTER_ISR(21);
    REGISTER_ISR(22);
    REGISTER_ISR(23);
    REGISTER_ISR(24);
    REGISTER_ISR(25);
    REGISTER_ISR(26);
    REGISTER_ISR(27);
    REGISTER_ISR(28);
    REGISTER_ISR(29);
    REGISTER_ISR(30);
#undef REGISTER_ISR

    // 使用idt_use_reg设置31号中断门的基地址为isr31
    idt_use_reg(31, (uint32_t)isr31);

#define REGISTER_IRQ(id, irq_id) idt_set_gate(id, (uint32_t)irq##irq_id, 0x08, 0x8E)
    // 为IRQ向量32到47注册处理函数
    REGISTER_IRQ(32, 0);
    REGISTER_IRQ(33, 1);
    REGISTER_IRQ(34, 2);
    REGISTER_IRQ(35, 3);
    REGISTER_IRQ(36, 4);
    REGISTER_IRQ(37, 5);
    REGISTER_IRQ(38, 6);
    REGISTER_IRQ(39, 7);
    REGISTER_IRQ(40, 8);
    REGISTER_IRQ(41, 9);
    REGISTER_IRQ(42, 10);
    REGISTER_IRQ(43, 11);
    REGISTER_IRQ(44, 12);
    REGISTER_IRQ(45, 13);
    REGISTER_IRQ(46, 14);
    REGISTER_IRQ(47, 15);
#undef REGISTER_IRQ

    // 刷新IDT，加载新的IDT到CPU中
    idt_flush((uint32_t) &idt_ptr);
}


