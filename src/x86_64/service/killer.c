#include "killer.h"
#include "fsgsbase.h"
#include "heap.h"
#include "klog.h"
#include "lock_queue.h"
#include "smp.h"

lock_queue        *death_proc_queue; // 死亡进程队列
extern lock_queue *pgb_queue;

_Noreturn void halt_service() {
    loop {
        open_interrupt;
        __asm__ volatile("hlt");
        if (!current_cpu->ready) continue;
        tcb_t task = NULL;
        spin_lock(current_cpu->death_queue->lock);
        queue_foreach(current_cpu->death_queue, node) {
            task = (tcb_t)node->data;
            if (task == NULL) continue;
            break;
        }
        spin_unlock(current_cpu->death_queue->lock);
        if (task == NULL || task->status != DEATH) { continue; }
        queue_remove_at(current_cpu->death_queue, task->death_index);
        kill_thread0(task);
        queue_remove_at(task->parent_group->pcb_queue, task->group_index);
        free(task);
    }
}

_Noreturn void killer_service() {
    loop {
        // 释放无线程进程
        spin_lock(pgb_queue->lock);
        pcb_t death_proc = NULL;
        queue_foreach(pgb_queue, node) {
            pcb_t process = (pcb_t)node->data;
            if (process == NULL) continue;
            if (process->pcb_queue->size == 0 && process->status == RUNNING) {
                death_proc = process;
            }
        }
        spin_unlock(pgb_queue->lock);
        if (death_proc != NULL) {
            logkf("Process %s has no thread, kill it.\n", death_proc->name);
            kill_proc(death_proc, 0);
        }
        if (death_proc_queue->size == 0) {
            __asm__ volatile("pause" ::: "memory");
            continue;
        }

        // 释放死亡进程
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
