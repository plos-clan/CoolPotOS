#include "eevdf.h"
#include "lock_queue.h"
#include "map.h"
#include "pcb.h"

map *futex_map = NULL;

void futex_init() {
    futex_map = map_create(20);
}

void futex_add(void *phys_addr, tcb_t thread) {
    lock_queue *queue = (lock_queue *)map_get(futex_map, phys_addr);
    if (queue == NULL) {
        queue = queue_init();
        map_set(futex_map, phys_addr, queue);
    }
    queue_enqueue(queue, thread);
    wait_eevdf_entity(thread, get_cpu_smp(thread->cpu_id));
}

void futex_wake(void *phys_addr, int count) {
    lock_queue *queue = (lock_queue *)map_get(futex_map, phys_addr);
    if (queue == NULL) { return; }

    for (int i = 0; i < count && queue->size != 0; i++) {
        tcb_t thread = queue_dequeue(queue);
        if (thread != NULL) {
            thread->status = START;
            futex_eevdf_entity(thread, get_cpu_smp(thread->cpu_id));
        }
    }

    if (queue->size == 0) {
        map_remove(futex_map, phys_addr);
        queue_destroy(queue);
    }
}

void futex_free(tcb_t thread) {
    if (thread == NULL) { return; }

    for (size_t i = 0; i < futex_map->size; i++) {
        lock_queue *queue = (lock_queue *)map_get(futex_map, futex_map->buckets[i]->key);
        if (queue != NULL) {
            queue_remove_at(queue, thread->futex_index);
            if (queue->size == 0) {
                map_remove(futex_map, futex_map->buckets[i]->key);
                queue_destroy(queue);
            }
        }
    }
}
