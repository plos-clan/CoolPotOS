#include "task/scheduler.h"
#include "task/smp.h"

bool add_task_prio(tcb_t thread, uint64_t prio) {
    if (thread == NULL) return false;
    cpu_local_t *local = get_min_task_count_cpu();
    local              = local == NULL ? arch_current_cpu() : local;
    if (local == NULL) return false;
    thread->prio = prio;
    add_eevdf_entity_with_prio(thread, prio, local);
    return true;
}

bool add_task_prio_cpu(tcb_t thread, uint64_t prio, cpu_local_t *cpu) {
    if (cpu == NULL || thread == NULL) return false;
    thread->prio = prio;
    add_eevdf_entity_with_prio(thread, prio, cpu);
    return true;
}

void set_cpu_idle_task(tcb_t thread, cpu_local_t *cpu) {
    thread->prio      = NICE_TO_PRIO(-20);
    cpu->idle_task    = thread;
    cpu->current_task = thread;
    init_cpu_idle(cpu,thread);
}

tcb_t pick_next_task(uint64_t cpu_id) {
    cpu_local_t *cpu_local   = get_cpu_local(cpu_id);
    tcb_t        next_thread = eevdf_pick_next_task(cpu_local);
    if (next_thread == NULL) next_thread = cpu_local->idle_task;
    return next_thread;
}
