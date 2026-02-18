#include "intctl.h"
#include "id_alloc.h"
#include "krlibc.h"
#include "task/smp.h"
#include "term/klog.h"

static id_allocator_t *intctl_irq_alloc;
static irq_action_t actions[ARCH_MAX_IRQ_NUM];

static _Atomic volatile uint64_t irq_count = 0;

uint64_t get_all_irq_count() {
    return irq_count;
}

irq_action_t *get_irq_actions() {
    return actions;
}

void do_irq(struct pt_regs *regs, const uint64_t irq_num) {
    irq_action_t *action = &actions[irq_num];

    const cpu_local_t *cpu = arch_current_cpu();
    action->int_count[cpu->id]++;

    if (action->handler) {
        action->handler(irq_num, action->data, regs);
        irq_count++;
    } else {
        printk("Intr vector [%d] does not have a handler\n", irq_num);
    }

    if (action->irq_controller && action->irq_controller->send_eoi) {
        action->irq_controller->send_eoi(irq_num);
    } else {
        printk("Intr vector [%d] does not have an ack\n", irq_num);
    }
}

void irq_regist_irq(
    const uint64_t irq_num,
    void (*handler)(uint64_t irq_num, void *data, struct pt_regs *regs),
    const uint64_t arg,
    void *data,
    intctl_t *controller,
    const char *name,
    const uint64_t flags,
    const enum irq_type type
) {
    irq_action_t *action = &actions[irq_num];

    action->handler        = handler;
    action->data           = data;
    action->irq_controller = controller;
    action->name           = strdup(name);
    action->type           = type;

    if (action->irq_controller && action->irq_controller->_install) {
        action->irq_controller->_install(irq_num, arg, flags);
    }

    if (action->irq_controller && action->irq_controller->_unmask) {
        action->irq_controller->_unmask(irq_num, flags);
    }

    action->flags = flags;
}

int irq_allocate_irqnum() {
    return id_alloc(intctl_irq_alloc);
}

void irq_deallocate_irqnum(const int irq_num) {
    id_free(intctl_irq_alloc, irq_num);
}

void irq_set_alloc(const size_t irq_num) {
    id_alloc_set(intctl_irq_alloc, irq_num);
}

void intctl_init() {
    intctl_irq_alloc = id_allocator_create(ARCH_MAX_IRQ_NUM);
}
