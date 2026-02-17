#include "task/scheduler.h"
#include "cow_arraylist.h"
#include "errno.h"
#include "intctl.h"
#include "krlibc.h"
#include "lock.h"
#include "task/smp.h"
#include "term/klog.h"
#include "timer.h"

#if EEVDF_SCHEDULER
#    include "task/eevdf.h"
#else
#    include "task/rrs.h"
#endif

_Atomic volatile bool scheduler_status = false;

static cow_arraylist *sleep_list = NULL;
static spin_t         sleep_lock = SPIN_INIT;

static inline void sleep_block_task(tcb_t thread) {
    cpu_local_t *cpu = get_cpu_local(thread->cpu_id);
#if EEVDF_SCHEDULER
    wait_eevdf_entity(thread, cpu);
#else
    remove_rrs_entity(thread, cpu);
#endif
}

static inline void sleep_wake_task(tcb_t thread) {
    cpu_local_t *cpu = get_cpu_local(thread->cpu_id);
#if EEVDF_SCHEDULER
    futex_eevdf_entity(thread, cpu);
#else
    add_rrs_entity(thread, cpu);
#endif
}

void scheduler_check_sleep() {
    if (sleep_list == NULL || sleep_list->size == 0)
        return;

    uint64_t now = nano_time();

    if (!spin_trylock(sleep_lock))
        return;
    for (size_t i = 0; i < sleep_list->size;) {
        tcb_t thread = (tcb_t)cow_list_get(sleep_list, i);
        if (thread == NULL) {
            cow_list_remove(sleep_list, i);
            continue;
        }

        bool wake = (now >= thread->sleep_deadline) || signals_pending_quick(thread);

        if (wake) {
            cow_list_remove(sleep_list, i);
            thread->sleep_deadline = 0;
            thread->status         = T_START;
            sleep_wake_task(thread);
        } else {
            i++;
        }
    }
    spin_unlock(sleep_lock);
}

void scheduler_enable() {
    scheduler_status = true;
}

void scheduler_disable() {
    scheduler_status = false;
}

int scheduler_nano_sleep(uint64_t nano) {
    if (sleep_list == NULL)
        sleep_list = cow_list_create();

    tcb_t current           = get_current_task();
    current->sleep_deadline = nano_time() + nano;
    current->status         = T_WAIT;

    bool int_enable = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(sleep_lock);
    cow_list_add(sleep_list, current);
    sleep_block_task(current);
    spin_unlock(sleep_lock);
    if (int_enable)
        arch_open_interrupt();

    scheduler_yield();

    // 被唤醒后检查是否因信号中断
    if (signals_pending_quick(current)) {
        return -EINTR;
    }
    return 0;
}

bool scheduler_add_task(tcb_t thread, uint64_t prio) {
    if (thread == NULL)
        return false;
    cpu_local_t *local = NULL; // get_min_task_count_cpu();
    local              = local == NULL ? arch_current_cpu() : local;
    if (local == NULL)
        return false;
    local->task_count++;
    thread->prio   = prio;
    thread->cpu_id = local->id;
#if EEVDF_SCHEDULER
    add_eevdf_entity_with_prio(thread, prio, local);
#else
    add_rrs_entity(thread, local);
#endif
    return true;
}

bool scheduler_add_task_cpu(tcb_t thread, uint64_t prio, cpu_local_t *cpu) {
    if (cpu == NULL || thread == NULL)
        return false;
    cpu->task_count++;
    thread->prio   = prio;
    thread->cpu_id = cpu->id;
#if EEVDF_SCHEDULER
    add_eevdf_entity_with_prio(thread, prio, cpu);
#else
    add_rrs_entity(thread, cpu);
#endif
    return true;
}

void scheduler_set_bsp_cpu(cpu_local_t *bsp_cpu) {
    extern tcb_t bsp_idle_thread;
    bsp_cpu->enable       = true;
    bsp_cpu->directory    = get_kernel_pagedir();
    bsp_cpu->current_task = bsp_idle_thread;
    bsp_cpu->is_yield     = false;
    bsp_cpu->jiffies      = 0;
    bsp_cpu->idle_jiffies = 0;
    bsp_cpu->task_count   = 1;

    bsp_idle_thread->prio   = NICE_TO_PRIO(0);
    bsp_cpu->idle_task      = bsp_idle_thread;
    bsp_cpu->current_task   = bsp_idle_thread;
    bsp_idle_thread->cpu_id = bsp_cpu->id;
#if EEVDF_SCHEDULER
    init_cpu_idle(bsp_cpu, bsp_idle_thread);
#else
    init_cpu_idle_rrs(bsp_cpu, bsp_idle_thread);
#endif
}

void scheduler_set_cpu_idle(tcb_t thread, cpu_local_t *cpu) {
    thread->prio      = NICE_TO_PRIO(-20);
    cpu->idle_task    = thread;
    cpu->current_task = thread;
    cpu->is_yield     = false;
    cpu->jiffies      = 0;
    cpu->idle_jiffies = 0;
    cpu->task_count   = 1;
    thread->cpu_id    = cpu->id;
#if EEVDF_SCHEDULER
    init_cpu_idle(cpu, thread);
#else
    init_cpu_idle_rrs(cpu, thread);
#endif
}

void scheduler_remove_task(tcb_t thread, cpu_local_t *cpu) {
    if (thread == NULL || cpu == NULL || thread->sched_handle == NULL)
        return;
    if (cpu->task_count > 0)
        cpu->task_count--;
    if (thread->status == T_WAIT && thread->sleep_deadline != 0) {
        // 从 sleep 列表中移除
        bool int_enable = arch_check_interrupt();
        arch_close_interrupt();
        spin_lock(sleep_lock);
        for (size_t i = 0; i < sleep_list->size; i++) {
            if (cow_list_get(sleep_list, i) == thread) {
                cow_list_remove(sleep_list, i);
                break;
            }
        }
        thread->sleep_deadline = 0;
        spin_unlock(sleep_lock);
        // 从等待队列移回运行队列再移除
#if EEVDF_SCHEDULER
        futex_eevdf_entity(thread, cpu);
        remove_eevdf_entity(thread, cpu);
#else
        add_rrs_entity(thread, cpu);
        remove_rrs_entity(thread, cpu);
#endif
        if (int_enable)
            arch_open_interrupt();
        return;
    }
    if (thread->status == T_FUTEX) {
        bool int_enable = arch_check_interrupt();
        arch_close_interrupt();
#if EEVDF_SCHEDULER
        futex_eevdf_entity(thread, cpu);
        remove_eevdf_entity(thread, cpu);
#else
        remove_rrs_entity(thread, cpu);
#endif
        if (int_enable)
            arch_open_interrupt();
        return;
    }
#if EEVDF_SCHEDULER
    remove_eevdf_entity(thread, cpu);
#else
    remove_rrs_entity(thread, cpu);
#endif
}

void scheduler_change_weight(tcb_t thread, uint64_t prio) {
#if EEVDF_SCHEDULER
    change_entity_weight(thread, prio, get_cpu_local(thread->cpu_id));
#endif
}

tcb_t scheduler_pick_next(uint64_t cpu_id) {
    cpu_local_t *cpu_local = get_cpu_local(cpu_id);
    tcb_t        next_thread =
#if EEVDF_SCHEDULER
        eevdf_pick_next_task(cpu_local);
#else
        rrs_pick_next_task(cpu_local);
#endif
    if (next_thread == NULL)
        next_thread = cpu_local->idle_task;
    return next_thread;
}

void scheduler_yield() {
    cpu_local_t *cpu = arch_current_cpu();
    if (unlikely(cpu == NULL))
        return;
    cpu->is_yield = true;

#if EEVDF_SCHEDULER
    set_entity_yield(get_current_task());
#endif
    arch_send_scheduler();
}

void scheduler_handler(uint64_t irq_num, void *data, struct pt_regs *regs) {
    if (!scheduler_status)
        return;
    cpu_local_t *cpu = arch_current_cpu();
    if (unlikely(cpu == NULL))
        return;

    if (!cpu->is_yield) {
        cpu->jiffies++;
        if (cpu->current_task == cpu->idle_task) {
            cpu->idle_jiffies++;
        }
    }
    cpu->is_yield = false;

    scheduler_check_sleep();

    tcb_t current_thread = get_current_task();
    if (unlikely(current_thread == NULL))
        return;
    tcb_t next_thread = scheduler_pick_next(cpu->id);

    extern pcb_t kernel_process;
    if (next_thread->process->parent == NULL || next_thread->process->parent->status == T_DEATH
        || next_thread->process->parent->status == T_OUT) {
        next_thread->process->parent = kernel_process;
    }

    if (current_thread == next_thread)
        return;
    current_thread->status = T_RUNNING;
    next_thread->status    = T_START;
    cpu->current_task      = next_thread;
    arch_task_switch(current_thread, next_thread, regs);
}

USED void foreach_all_process() {
    extern cow_arraylist *process_list;
    pcb_t                 proc = NULL;
    cow_foreach(process_list, proc) {
        logkf("process name: %s, pid: %d\n", proc->name, proc->pid);
    }
}
