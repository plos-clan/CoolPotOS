#include "task/scheduler.h"
#include "intctl.h"
#include "task/smp.h"
#include "term/klog.h"
#include "timer.h"

// #define EEVDF_SCHEDULER 1

#ifdef EEVDF_SCHEDULER
#    include "task/eevdf.h"
#else
#    include "task/rrs.h"
#endif

_Atomic volatile bool scheduler_status = false;

void enable_scheduler() {
    scheduler_status = true;
}

void disable_scheduler() {
    scheduler_status = false;
}

void scheduler_nano_sleep(uint64_t nano) {
    uint64_t targetTime        = nano_time();
    uint64_t after             = 0;
    get_current_task()->status = T_WAIT;
    while (true) {
        uint64_t n = nano_time();
        if (n < targetTime) {
            after      += UINT64_MAX - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) {
            get_current_task()->status = T_RUNNING;
            return;
        }
        if (nano > 10) {
            scheduler_yield(); // 让出CPU时间片
        }
    }
}

bool add_task_prio(tcb_t thread, uint64_t prio) {
    if (thread == NULL) return false;
    cpu_local_t *local = get_min_task_count_cpu();
    local              = local == NULL ? arch_current_cpu() : local;
    if (local == NULL) return false;
    thread->prio   = prio;
    thread->cpu_id = local->id;
#ifdef EEVDF_SCHEDULER
    add_eevdf_entity_with_prio(thread, prio, local);
#else
    add_rrs_entity(thread, local);
#endif
    return true;
}

bool add_task_prio_cpu(tcb_t thread, uint64_t prio, cpu_local_t *cpu) {
    if (cpu == NULL || thread == NULL) return false;
    thread->prio   = prio;
    thread->cpu_id = cpu->id;
#ifdef EEVDF_SCHEDULER
    add_eevdf_entity_with_prio(thread, prio, cpu);
#else
    add_rrs_entity(thread, cpu);
#endif
    return true;
}

void set_bsp_cpu_info(cpu_local_t *bsp_cpu) {
    extern tcb_t bsp_idle_thread;
    bsp_cpu->enable       = true;
    bsp_cpu->directory    = get_kernel_pagedir();
    bsp_cpu->current_task = bsp_idle_thread;

    bsp_idle_thread->prio   = NICE_TO_PRIO(0);
    bsp_cpu->idle_task      = bsp_idle_thread;
    bsp_cpu->current_task   = bsp_idle_thread;
    bsp_idle_thread->cpu_id = bsp_cpu->id;
#ifdef EEVDF_SCHEDULER
    init_cpu_idle(bsp_cpu, bsp_idle_thread);
#else
    init_cpu_idle_rrs(bsp_cpu, bsp_idle_thread);
#endif
}

void set_cpu_idle_task(tcb_t thread, cpu_local_t *cpu) {
    thread->prio      = NICE_TO_PRIO(-20);
    cpu->idle_task    = thread;
    cpu->current_task = thread;
    thread->cpu_id    = cpu->id;
#ifdef EEVDF_SCHEDULER
    init_cpu_idle(cpu, thread);
#else
    init_cpu_idle_rrs(cpu, thread);
#endif
}

void remove_task(tcb_t thread, cpu_local_t *cpu) {
    if (thread->status == T_FUTEX) {
        bool int_enable = arch_check_interrupt();
        arch_close_interrupt();
#ifdef EEVDF_SCHEDULER
        futex_eevdf_entity(thread, cpu);
        remove_eevdf_entity(thread, cpu);
#else
        remove_rrs_entity(thread, cpu);
#endif
        if (int_enable) arch_open_interrupt();
        return;
    }
#ifdef EEVDF_SCHEDULER
    remove_eevdf_entity(thread, cpu);
#else
    remove_rrs_entity(thread, cpu);
#endif
}

void change_task_weight(tcb_t thread, uint64_t prio) {
#ifdef EEVDF_SCHEDULER
    change_entity_weight(thread, prio, get_cpu_local(thread->cpu_id));
#endif
}

tcb_t pick_next_task(uint64_t cpu_id) {
    cpu_local_t *cpu_local = get_cpu_local(cpu_id);
    tcb_t        next_thread =
#ifdef EEVDF_SCHEDULER
        eevdf_pick_next_task(cpu_local);
#else
        rrs_pick_next_task(cpu_local);
#endif
    if (next_thread == NULL) next_thread = cpu_local->idle_task;
    return next_thread;
}

void scheduler_yield() {
#ifdef EEVDF_SCHEDULER
    set_entity_yield(get_current_task());
#endif
    arch_send_scheduler();
}

void scheduler_handler(uint64_t irq_num, void *data, struct pt_regs *regs) {
    if (!scheduler_status) return;
    cpu_local_t *cpu = arch_current_cpu();
    if (unlikely(cpu == NULL)) return;
    tcb_t current_thread = get_current_task();
    if (unlikely(current_thread == NULL)) return;
    tcb_t next_thread = pick_next_task(cpu->id);

    extern pcb_t kernel_process;
    if (next_thread->process->parent == NULL || next_thread->process->parent->status == T_DEATH ||
        next_thread->process->parent->status == T_OUT) {
        next_thread->process->parent = kernel_process;
    }

    if (current_thread == next_thread) return;
    current_thread->status = T_RUNNING;
    next_thread->status    = T_START;
    cpu->current_task      = next_thread;
    arch_task_switch(current_thread, next_thread, regs);
}
