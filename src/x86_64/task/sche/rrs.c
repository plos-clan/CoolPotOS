/*
 * CP_Kernel RRS 循环公平调度
 */
#include "fsgsbase.h"
#include "pcb.h"
#include "scheduler.h"
#include "smp.h"

tcb_t select_next_task_rrs() {
    tcb_t next;
    if (current_cpu->scheduler_queue->size == 1) {
        write_fsbase(get_current_task()->fs_base); // 没有进行任务切换，再换回来
        return get_current_task();
    }
    if (current_cpu->iter_node == NULL) {
    iter_head:
        current_cpu->iter_node = current_cpu->scheduler_queue->head;
        next                   = (tcb_t)current_cpu->iter_node->data;
    } else {
    resche:
        current_cpu->iter_node = current_cpu->iter_node->next;
        if (current_cpu->iter_node == NULL) goto iter_head;
        next = (tcb_t)current_cpu->iter_node->data;
        not_null_assets(next, "scheduler next null");

        // 死亡/回收/挂起的线程不调度
        switch (next->status) {
        case FUTEX:
        case DEATH:
        case OUT: goto resche;
        default:
            if (next->parent_group->status == DEATH || next->parent_group->status == FUTEX) {
                goto resche;
            }
        }
    }
    return next;
}