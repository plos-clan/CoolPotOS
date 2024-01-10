#include "../include/idt.h"
#include "../include/io.h"
#include "../include/console.h"
#include "../include/isr.h"
#include "../include/memory.h"
#include "../include/redpane.h"

static isr_t interrupt_handlers[256];
idt_entry_t idt_entries[256]; // IDT有256个描述符
idt_ptr_t idt_ptr;
extern void idt_flush(uint32_t);

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF; // 拆成低位和高位

    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
    // flags | 0x60，即可被Ring3调用
}

void idt_install(){
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint32_t) &idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0); // 初始化PIC，将IRQ0~15对应至IDT0x20~0x2f

#define REGISTER_ISR(id) idt_set_gate(id, (uint32_t) isr##id, 0x08, 0x8E)
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
    REGISTER_ISR(31);
#undef REGISTER_ISR
#define REGISTER_IRQ(id, irq_id) idt_set_gate(id, (uint32_t) irq##irq_id, 0x08, 0x8E)
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
    idt_flush((uint32_t) &idt_ptr);
}

void isr_handler(registers_t regs) {
    terminal_writestring("[Kernel]: received interrupt: ");
    terminal_write_dec(regs.int_no);
    terminal_putchar('\n');

    if (interrupt_handlers[regs.int_no]) {
        isr_t handler = interrupt_handlers[regs.int_no]; // 有自定义处理程序，调用之
        handler(&regs); // 传入寄存器
    }
}

void irq_handler(registers_t regs) {
    if (regs.int_no >= 40) outb(0xA0, 0x20); // 中断号 >= 40，来自从片，发送EOI给从片
    outb(0x20, 0x20); // 发送EOI给主片

    if (interrupt_handlers[regs.int_no]) {
        isr_t handler = interrupt_handlers[regs.int_no]; // 有自定义处理程序，调用之
        handler(&regs); // 传入寄存器
    }
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void div_over(registers_t *reg){
    show_VGA_red_pane("DIV_OVER","Division overflow",0x00,reg);
}

void error_setup(){
    register_interrupt_handler(0x00,div_over);
}



