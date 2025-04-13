#pragma once

#include "ctype.h"

enum InterruptIndex {
    timer = 32,
    keyboard,
    mouse,
    hpet_timer,
    ide_primary,
    ide_secondary,
    pcnet,
    xhci,
	sb16,
};

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static inline struct interrupt_frame get_current_registers() {
    struct interrupt_frame state;
    __asm__ volatile("lea (%%rip), %0\n"
                     "mov %%cs, %1\n"
                     "pushfq\n"
                     "pop %2\n"
                     "mov %%rsp, %3\n"
                     "mov %%ss, %4\n"
                     : "=r"(state.rip), "=r"(state.cs), "=r"(state.rflags), "=r"(state.rsp),
                       "=r"(state.ss)
                     :
                     : "memory");
    return state;
}

typedef struct interrupt_frame interrupt_frame_t;
void kernel_error(const char *msg, uint64_t code, interrupt_frame_t *frame);
void print_register(interrupt_frame_t *frame);

/**
 * 注册一个中断处理函数
 * @param vector 中断向量号
 * @param handler 处理函数
 * @param ist (无特殊需求填0)
 * @param flags 中断描述符标志位
 */
void register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags);

/**
 * 设置当前CPU核心的TSS段中内核栈
 * @param rsp 栈基址
 */
void set_kernel_stack(uint64_t rsp);

/**
 * 向APIC发送EOI
 */
void send_eoi();

/**
 * 停止该CPU核心的LAPIC定时器
 */
void lapic_timer_stop();
