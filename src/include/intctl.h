#pragma once

#if defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    define ARCH_MAX_IRQ_NUM 1020
#    define IRQ_BASE_VECTOR 32
#elif defined(__aarch64__)
#    define ARCH_MAX_IRQ_NUM 1020
#    define IRQ_BASE_VECTOR 32
#elif defined(__loongarch__) || defined(__loongarch64)
#    define ARCH_MAX_IRQ_NUM 1024
#    define IRQ_BASE_VECTOR 32
#elif defined(__x86_64__) || defined(__amd64__)
#    define ARCH_MAX_IRQ_NUM 256
#    define IRQ_BASE_VECTOR 32
#endif

#define IRQ_FLAGS_MSIX (1UL << 0)

#include "metadata.h"
#include "ptrace.h"
#include "types.h"

enum irq_num_index {
    timer = IRQ_BASE_VECTOR,
    ps2_kbd,
    power,
};

enum irq_type {
    PCI_MSI,
    IO_APIC,
};

typedef struct intctl {
    int64_t (*send_eoi)(uint64_t irq);
    int64_t (*_mask)(uint64_t irq, uint64_t flags);
    int64_t (*_unmask)(uint64_t irq, uint64_t flags);
    int64_t (*_install)(uint64_t vector, uint64_t irq, uint64_t flags);
} intctl_t;

typedef struct irq_action {
    char *name;
    void *data;
    intctl_t *irq_controller;
    void (*handler)(uint64_t irq_num, void *data, struct pt_regs *regs);
    uint64_t flags;
    enum irq_type type;
    size_t int_count[MAX_CPU];
} irq_action_t;

void intctl_init();

void irq_regist_irq(
    uint64_t irq_num, void (*handler)(uint64_t irq_num, void *data, struct pt_regs *regs),
    uint64_t arg, void *data, intctl_t *controller, char *name, uint64_t flags, enum irq_type type
);
void do_irq(struct pt_regs *regs, uint64_t irq_num);

uint64_t get_all_irq_count();
int irq_allocate_irqnum();
void irq_deallocate_irqnum(int irq_num);
void irq_set_alloc(size_t irq_num); // 用于设备已经注册的IRQ设置占用, 防止自动分配覆盖IRQ
