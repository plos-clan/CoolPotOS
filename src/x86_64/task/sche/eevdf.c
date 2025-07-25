/*
 * CP_Kernel EEVDF 多核负载公平调度算法
 */
#include "fsgsbase.h"
#include "pcb.h"
#include "scheduler.h"
#include "smp.h"

size_t scale_inverse_fair(tcb_t t, double value) {
    return value * 100 / t->weight;
}

void compute_lag() {
    size_t total_vruntime = 0.0;
    queue_foreach(current_cpu->scheduler_queue, node) {
        tcb_t tcb1      = (tcb_t)node->data;
        total_vruntime += tcb1->vruntime;
    }
    size_t avg_vruntime = total_vruntime / current_cpu->scheduler_queue->size;
    queue_foreach(current_cpu->scheduler_queue, node) {
        tcb_t t = (tcb_t)node->data;
        t->lag  = avg_vruntime - t->vruntime;
    }
}

tcb_t pick_next_task() {
    tcb_t selected = NULL;
    queue_foreach(current_cpu->scheduler_queue, node) {
        tcb_t t = (tcb_t)node->data;
        if (t->status == FUTEX || t->status == DEATH || t->status == OUT) continue;
        if (t->lag >= 0) {
            if (selected == NULL || t->deadline < selected->deadline) { selected = t; }
        }
    }
    return selected;
}

tcb_t select_next_task_eevdf() {
    compute_lag();
    tcb_t best = pick_next_task();

    int runtime     = best->use_slice;
    best->vruntime += scale_inverse_fair(best, runtime);
    best->deadline  = best->vruntime + scale_inverse_fair(best, best->time_slice);
    return best;
}
