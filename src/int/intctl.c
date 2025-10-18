#include "intctl.h"
#include "id_alloc.h"
#include "term/klog.h"
#include "krlibc.h"

id_allocator_t *intctl_irq_alloc;
irq_action_t    actions[ARCH_MAX_IRQ_NUM];

void do_irq(struct pt_regs *regs, uint64_t irq_num) {
    irq_action_t *action = &actions[irq_num];

    if (action->handler) {
        action->handler(irq_num, action->data, regs);
    } else {
        printk("Intr vector [%d] does not have a handler\n", irq_num);
    }

    if (action->irq_controller && action->irq_controller->send_eoi) {
        action->irq_controller->send_eoi(irq_num);
    } else {
        printk("Intr vector [%d] does not have an ack\n", irq_num);
    }

    //TODO scheduler irq
}

void irq_regist_irq(uint64_t irq_num,
                    void (*handler)(uint64_t irq_num, void *data, struct pt_regs *regs),
                    uint64_t arg, void *data, intctl_t *controller, char *name) {
    irq_action_t *action = &actions[irq_num];

    action->handler        = handler;
    action->data           = data;
    action->irq_controller = controller;
    action->name           = strdup(name);

    if (action->irq_controller && action->irq_controller->_install) {
        action->irq_controller->_install(irq_num, arg);
    }

    if (action->irq_controller && action->irq_controller->_unmask) {
        action->irq_controller->_unmask(irq_num);
    }
}

int irq_allocate_irqnum() {
    return id_alloc(intctl_irq_alloc);
}

void irq_deallocate_irqnum(int irq_num) {
    id_free(intctl_irq_alloc, irq_num);
}

void intctl_init() {
    intctl_irq_alloc = id_allocator_create(ARCH_MAX_IRQ_NUM);
}
