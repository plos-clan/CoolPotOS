#include "error.h"
#include "isr.h"
#include "scheduler.h"
#include "klog.h"
#include "io.h"

static void DE_0(registers_t *reg) {
    pcb_t *pcb = get_current_proc();
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        klogf(false, "Kernel has divide error.\n");
        io_cli();
        while (1) io_hlt();
    } else {
        klogf(false, "[%s:%d] has divide error.\n", pcb->name, pcb->pid);
        kill_proc(pcb);
    }
}

static void SS_12(registers_t *reg) {
    pcb_t *pcb = get_current_proc();
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        klogf(false, "Kernel has stack segment fault.\n");
        io_cli();
        while (1) io_hlt();
    } else {
        klogf(false, "[%s:%d] has stack segment fault.\n", pcb->name, pcb->pid);
        kill_proc(pcb);
    }
}

static void GP_13(registers_t *reg) {
    pcb_t *pcb = get_current_proc();
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        klogf(false, "Kernel has General Protection.\n");
        io_cli();
        while (1) io_hlt();
    } else {
        klogf(false, "[%s:%d] has General Protection.\n", pcb->name, pcb->pid);
        kill_proc(pcb);
    }
}

void setup_error() {
    register_interrupt_handler(0, DE_0);
    register_interrupt_handler(12, SS_12);
    register_interrupt_handler(13, GP_13);
}