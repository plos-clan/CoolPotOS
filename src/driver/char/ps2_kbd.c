#include "driver/char/ps2_kbd.h"
#include "driver/uacpi/resources.h"
#include "driver/uacpi/utilities.h"
#include "intctl.h"
#include "term/klog.h"

#if defined(__x86_64__) || defined(__amd64__)
#include "io.h"
#endif

static uint64_t ps2_kbd_irq = -1;

static void ps2_key_handle(uint64_t irq, void *arg, struct pt_regs *regs) {
    uint8_t scancode = 0;
#if defined(__x86_64__) || defined(__amd64__)
    scancode = io_in8(0x60);
#endif
    logkf("ps/2 key_code: %llu\n\r",scancode);
}

void ps2k_create_device() {
#if defined(__x86_64__) || defined(__amd64__)
    extern intctl_t apic_controller;
    irq_regist_irq(ps2_kbd_irq + IRQ_BASE_VECTOR, ps2_key_handle, ps2_kbd_irq, NULL,
                   &apic_controller, "sched_handle");
#endif
}

static uacpi_iteration_decision iteration_decision(void *user, uacpi_resource *resource) {
    if (resource == NULL) return UACPI_ITERATION_DECISION_BREAK;
    if (resource->type == UACPI_RESOURCE_TYPE_IRQ) {
        for (uacpi_u32 i = 0; i < resource->irq.num_irqs; i++) {
            uint64_t current_irq = resource->irq.irqs[i];
            if (current_irq == 1) {
                ps2_kbd_irq = current_irq;
                kinfo("found PS/2 keyboard IRQ: %llu", ps2_kbd_irq);
                return UACPI_ITERATION_DECISION_BREAK;
            }
        }
    }
    return UACPI_ITERATION_DECISION_NEXT_PEER;
}

static uacpi_iteration_decision match_ps2k(void *user, uacpi_namespace_node *node, uacpi_u32 i) {
    uacpi_resources *kb_res;
    uacpi_status     ret = uacpi_get_current_resources(node, &kb_res);
    if (uacpi_unlikely_error(ret)) {
        kwarn("unable to retrieve PS2K resources: %s", uacpi_status_to_string(ret));
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }
    uacpi_for_each_resource(kb_res, iteration_decision, user);
    ps2k_create_device();
    uacpi_free_resources(kb_res);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void ps2_kdb_setup() {
    uacpi_find_devices(PS2K_PNP_ID, match_ps2k, NULL);
}
