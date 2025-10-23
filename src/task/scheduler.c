#include "task/scheduler.h"
#include "task/smp.h"
#include "term/klog.h"
#include "task/eevdf.h"

_Atomic volatile bool scheduler_status = false;

void enable_scheduler() {
    scheduler_status = true;
}

void disable_scheduler() {
    scheduler_status = false;
}

bool add_task_prio(tcb_t thread, uint64_t prio) {
    if (thread == NULL) return false;
    cpu_local_t *local = get_min_task_count_cpu();
    local              = local == NULL ? arch_current_cpu() : local;
    if (local == NULL) return false;
    thread->prio   = prio;
    thread->cpu_id = local->id;
    add_eevdf_entity_with_prio(thread, prio, local);
    return true;
}

bool add_task_prio_cpu(tcb_t thread, uint64_t prio, cpu_local_t *cpu) {
    if (cpu == NULL || thread == NULL) return false;
    thread->prio   = prio;
    thread->cpu_id = cpu->id;
    add_eevdf_entity_with_prio(thread, prio, cpu);
    return true;
}

void set_cpu_idle_task(tcb_t thread, cpu_local_t *cpu) {
    thread->prio      = NICE_TO_PRIO(-20);
    cpu->idle_task    = thread;
    cpu->current_task = thread;
    thread->cpu_id    = cpu->id;
    init_cpu_idle(cpu, thread);
}

void remove_task(tcb_t thread, cpu_local_t *cpu) {
    if(thread->status == T_FUTEX){
        bool int_enable = arch_check_interrupt();
        arch_close_interrupt();
        futex_eevdf_entity(thread,cpu);
        remove_eevdf_entity(thread,cpu);
        if(int_enable) arch_open_interrupt();
        return;
    }
    remove_eevdf_entity(thread, cpu);
}

void change_task_weight(tcb_t thread, uint64_t prio) {
    change_entity_weight(thread, prio, get_cpu_local(thread->cpu_id));
}

tcb_t pick_next_task(uint64_t cpu_id) {
    cpu_local_t *cpu_local   = get_cpu_local(cpu_id);
    tcb_t        next_thread = eevdf_pick_next_task(cpu_local);
    if (next_thread == NULL) next_thread = cpu_local->idle_task;
    return next_thread;
}

void scheduler_yield() {
    //TODO yield impl
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
