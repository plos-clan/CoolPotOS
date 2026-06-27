#include "interrupt.h"
#include "io.h"
#include "krlibc.h"
#include "term/kprint.h"
#include "task/task.h"

/* 中断处理函数表 */
static interrupt_handler_t g_handlers[256];
static spin_t g_int_lock = SPIN_INIT;

/* 异常名称 (按异常号索引，最多20个) */
static const char *exception_names[20] = {
    [0]  = "Division Error",
    [1]  = "Debug",
    [2]  = "NMI",
    [3]  = "Breakpoint",
    [4]  = "Overflow",
    [5]  = "Bound Range Exceeded",
    [6]  = "Invalid Opcode",
    [7]  = "Device Not Available",
    [8]  = "Double Fault",
    [9]  = "Coprocessor Segment Overrun",
    [10] = "Invalid TSS",
    [11] = "Segment Not Present",
    [12] = "Stack Fault",
    [13] = "General Protection Fault",
    [14] = "Page Fault",
    [15] = "(Reserved)",
    [16] = "x87 FPU Error",
    [17] = "Alignment Check",
    [18] = "Machine Check",
    [19] = "SIMD FPU Exception",
};

/* PIC 端口 */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

/* 重映射 PIC */
static void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t mask1 = io_in8(PIC1_DATA);
    uint8_t mask2 = io_in8(PIC2_DATA);

    io_out8(PIC1_CMD, 0x11);  /* ICW1: 初始化 */
    io_out8(PIC2_CMD, 0x11);
    io_out8(PIC1_DATA, offset1); /* ICW2: 向量偏移 */
    io_out8(PIC2_DATA, offset2);
    io_out8(PIC1_DATA, 0x04);   /* ICW3: PIC1 连接 PIC2 在 IRQ2 */
    io_out8(PIC2_DATA, 0x02);   /* ICW3: PIC2 是级联 */
    io_out8(PIC1_DATA, 0x01);   /* ICW4: 8086 模式 */
    io_out8(PIC2_DATA, 0x01);

    io_out8(PIC1_DATA, mask1);
    io_out8(PIC2_DATA, mask2);
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        io_out8(PIC2_CMD, PIC_EOI);
    io_out8(PIC1_CMD, PIC_EOI);
}

void exception_handler(interrupt_frame_t *frame) {
    const char *name = (frame->int_no < 20 && exception_names[frame->int_no])
        ? exception_names[frame->int_no] : "Unknown";

    kerror("=== EXCEPTION ===");
    kerror("Exception: %s (vector=%llu)", name, frame->int_no);
    kerror("Error code: %llu", frame->err_code);
    kerror("RIP: 0x%llx  CS: 0x%llx  RFLAGS: 0x%llx", frame->rip, frame->cs, frame->rflags);
    kerror("RSP: 0x%llx  SS: 0x%llx", frame->rsp, frame->ss);
    kerror("RAX: 0x%llx  RBX: 0x%llx  RCX: 0x%llx", frame->rax, frame->rbx, frame->rcx);
    kerror("RDX: 0x%llx  RSI: 0x%llx  RDI: 0x%llx", frame->rdx, frame->rsi, frame->rdi);
    kerror("RBP: 0x%llx  R8: 0x%llx   R9: 0x%llx", frame->rbp, frame->r8, frame->r9);

    /* 对于页错误，额外输出 CR2 */
    if (frame->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        kerror("CR2 (faulting address): 0x%llx", cr2);
    }

    kerror("System halted.");

    /* 停止系统 */
    arch_close_interrupt();
    while (1) arch_wait_for_interrupt();
}

void isr_common_handler(interrupt_frame_t *frame) {
    /* 调用注册的处理函数 */
    if (frame->int_no < 256 && g_handlers[frame->int_no]) {
        g_handlers[frame->int_no](frame);
    } else {
        exception_handler(frame);
    }
}

void irq_common_handler(interrupt_frame_t *frame) {
    /* 调用注册的处理函数 */
    if (frame->int_no < 256 && g_handlers[frame->int_no]) {
        g_handlers[frame->int_no](frame);
    }

    pic_send_eoi((uint8_t)(frame->int_no - 32));
}

int interrupt_register(uint32_t vector, interrupt_handler_t handler) {
    if (vector >= 256 || !handler) return -EINVAL;
    spin_lock(g_int_lock);
    g_handlers[vector] = handler;
    spin_unlock(g_int_lock);
    return 0;
}

int interrupt_unregister(uint32_t vector) {
    if (vector >= 256) return -EINVAL;
    spin_lock(g_int_lock);
    g_handlers[vector] = NULL;
    spin_unlock(g_int_lock);
    return 0;
}

void interrupt_enable(void) {
    arch_open_interrupt();
}

void interrupt_disable(void) {
    arch_close_interrupt();
}

/* 定时器中断处理 */
static void timer_handler(interrupt_frame_t *frame) {
    (void)frame;
    scheduler_tick();
}

void interrupt_init(void) {
    memset(g_handlers, 0, sizeof(g_handlers));

    /* 重映射 PIC: IRQ 0-15 → 向量 32-47 */
    pic_remap(0x20, 0x28);

    /* 注册定时器中断 */
    interrupt_register(32 + IRQ_TIMER, timer_handler);

    /* 启用所有 IRQ */
    io_out8(PIC1_DATA, 0xFC);  /* 仅启用 IRQ0 (定时器) 和 IRQ1 (键盘) */
    io_out8(PIC2_DATA, 0xFF);  /* 禁用所有从 PIC */

    kinfo("Interrupt: subsystem initialized");
}