#include "description_table.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/lazy_alloc.h"
#include "ptrace.h"
#include "task/task.h"
#include "term/klog.h"

__IRQHANDLER void divide_error(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("divide_error: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("divide_error: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void debug_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("debug_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("debug_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void nmi_interrupt(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("nmi_interrupt: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("nmi_interrupt: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void breakpoint_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("breakpoint_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("breakpoint_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void overflow_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("overflow_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("overflow_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void bound_range_exceeded(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("bound_range_exceeded: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("bound_range_exceeded: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void invalid_opcode(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("invalid_opcode: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("invalid_opcode: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void device_not_available(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("device_not_available: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("device_not_available: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void double_fault(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("double_fault: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("double_fault: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void invalid_tss(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("invalid_tss: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("invalid_tss: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void segment_not_present(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("segment_not_present: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("segment_not_present: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void stack_segment_fault(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("stack_segment_fault: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("stack_segment_fault: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void general_protection_fault(struct interrupt_frame *frame, uint64_t error_code) {

    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("general_protection_fault: %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("general_protection_fault: %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}


USED volatile int is_debug;

__IRQHANDLER void page_fault_(struct interrupt_frame *frame, uint64_t error_code) {
    arch_close_interrupt();
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    if (likely(get_current_task() != NULL)) {
        if (get_current_task()->process->pid == 0) goto msg;
        errno_t status = lazy_tryalloc(get_current_task()->process, faulting_address);
        if (status == EOK) {
            arch_open_interrupt();
            return;
        }
        logkf("page_fault process(%s:%d) thread %s:%d\n", get_current_task()->process->name,
              get_current_task()->process->pid, get_current_task()->name, get_current_task()->tid);
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) kill_proc(process, -1, true);
        goto wfi;
    }
msg:;
    char *error_msg = !(error_code & 0x1) ? "NotPresent"
                      : error_code & 0x2  ? "WriteError"
                      : error_code & 0x4  ? "UserMode"
                      : error_code & 0x8  ? "ReservedBitsSet"
                      : error_code & 0x10 ? "DecodeAddress"
                                          : "Unknown";
    kerror("Page %s fault %p at %p", error_msg, faulting_address, frame->rip);
    if (get_current_task() != NULL) {
        printk("Current process(%s:%d) thread %s:%d\n", get_current_task()->process->name,
               get_current_task()->process->pid, get_current_task()->name, get_current_task()->tid);
    }
    arch_close_interrupt();
wfi:
    if (is_debug) return;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void x87_floating_point_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("x87_floating_point_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("x87_floating_point_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void alignment_check_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("alignment_check_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("alignment_check_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void machine_check_exception(struct interrupt_frame *frame, uint64_t error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("machine_check_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("machine_check_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

__IRQHANDLER void simd_floating_point_exception(struct interrupt_frame *frame,
                                                uint64_t                error_code) {
    if (get_current_task() != NULL) {
        pcb_t process = get_current_task()->process;
        if (process->pid != 0) {
            logkf("simd_floating_point_exception: error_code %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r", process->name, process->pid,
                  get_current_task()->name, get_current_task()->tid);
            kill_proc(process, -1, true);
            goto err;
        }
    } else
        arch_close_interrupt();

    kerror("simd_floating_point_exception: error_code %x at %p", error_code, frame->rip);
err:;
    while (true)
        arch_wait_for_interrupt();
}

void init_err_handle() {
    register_interrupt_handler(0, divide_error, 0, 0x8E);
    register_interrupt_handler(1, debug_exception, 0, 0x8E);
    register_interrupt_handler(2, nmi_interrupt, 0, 0x8E);
    register_interrupt_handler(3, breakpoint_exception, 0, 0x8E);
    register_interrupt_handler(4, overflow_exception, 0, 0x8E);
    register_interrupt_handler(5, bound_range_exceeded, 0, 0x8E);
    register_interrupt_handler(6, invalid_opcode, 0, 0x8E);
    register_interrupt_handler(7, device_not_available, 0, 0x8E);
    register_interrupt_handler(8, double_fault, 0, 0x8E);
    // Skip exception 9 (x87 FPU has been removed in x86_64)
    register_interrupt_handler(10, invalid_tss, 0, 0x8E);
    register_interrupt_handler(11, segment_not_present, 0, 0x8E);
    register_interrupt_handler(12, stack_segment_fault, 0, 0x8E);
    register_interrupt_handler(13, general_protection_fault, 0, 0x8E);
    register_interrupt_handler(14, page_fault_, 0, 0x8E);
    register_interrupt_handler(16, x87_floating_point_exception, 0, 0x8E);
    register_interrupt_handler(17, alignment_check_exception, 0, 0x8E);
    register_interrupt_handler(18, machine_check_exception, 0, 0x8E);
    register_interrupt_handler(19, simd_floating_point_exception, 0, 0x8E);
}
