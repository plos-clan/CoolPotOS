#include "killer.h"
#include "heap.h"
#include "lock_queue.h"
#include "smp.h"

lock_queue *death_proc_queue; // 死亡进程队列

_Noreturn void halt_service() {
    infinite_loop {
        __asm__ volatile("hlt");
        if (!cpu->ready) continue;
        tcb_t task = NULL;
        spin_lock(cpu->death_queue->lock);
        queue_foreach(cpu->death_queue, node) {
            task = (tcb_t)node->data;
            if (task == NULL) continue;
            break;
        }
        spin_unlock(cpu->death_queue->lock);
        if (task == NULL || task->status != DEATH) { continue; }
        queue_remove_at(cpu->death_queue, task->death_index);
        kill_thread0(task);
        queue_remove_at(task->parent_group->pcb_queue, task->group_index);
        free(task);
    }
}

_Noreturn void killer_service() {
    while (1) {
        if (death_proc_queue->size == 0) {
            __asm__ volatile("pause" ::: "memory");
            continue;
        }
        pcb_t task = NULL;
        queue_foreach(death_proc_queue, node) {
            task = (pcb_t)node->data;
            if (task == NULL) continue;
            bool is_out = true;
            spin_lock(task->pcb_queue->lock);
            queue_foreach(task->pcb_queue, node0) {
                tcb_t thread = (tcb_t)node0->data;
                if (thread == NULL) continue;
                if (thread->status != OUT) {
                    is_out = false;
                    break;
                }
            }
            spin_unlock(task->pcb_queue->lock);
            if (is_out) break;
            task = NULL;
        }
        if (task == NULL) continue;
        queue_remove_at(death_proc_queue, task->death_index);
        kill_proc0(task);
    }
}

void add_death_proc(pcb_t task) {
    if (task == NULL) return;
    task->death_index = lock_queue_enqueue(death_proc_queue, task);
    spin_unlock(death_proc_queue->lock);
}

void killer_setup() {
    death_proc_queue = queue_init();
    create_kernel_thread((void *)killer_service, NULL, "KillerService", NULL);
}
