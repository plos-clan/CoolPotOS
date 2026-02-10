#include "task/futex.h"
#include "task/smp.h"
#include "task/scheduler.h"
#if EEVDF_SCHEDULER
#    include "task/eevdf.h"
#else
#    include "task/rrs.h"
#endif
#include "cow_arraylist.h"
#include "map.h"
#include "term/klog.h"

map *futex_map = NULL;

static inline void futex_block_task(tcb_t thread) {
    cpu_local_t *cpu = get_cpu_local(thread->cpu_id);
#if EEVDF_SCHEDULER
    wait_eevdf_entity(thread, cpu);
#else
    remove_rrs_entity(thread, cpu);
#endif
}

static inline void futex_wake_task(tcb_t thread) {
    cpu_local_t *cpu = get_cpu_local(thread->cpu_id);
#if EEVDF_SCHEDULER
    futex_eevdf_entity(thread, cpu);
#else
    add_rrs_entity(thread, cpu);
#endif
}

void futex_init() {
    futex_map = map_create(20);
}

void futex_add(void *phys_addr, tcb_t thread) {
    cow_arraylist *queue = (cow_arraylist *)map_get(futex_map, phys_addr);
    if (queue == NULL) {
        queue = cow_list_create();
        map_set(futex_map, phys_addr, queue);
    }
    thread->futex_index = cow_list_add(queue, thread);
    futex_block_task(thread);
}

int futex_wake(void *phys_addr, int count) {
    cow_arraylist *queue = (cow_arraylist *)map_get(futex_map, phys_addr);
    if (queue == NULL || count <= 0) {
        return 0;
    }

    int woken = 0;
    for (size_t index = 0; index < queue->size && woken < count;) {
        tcb_t thread = (tcb_t)cow_list_get(queue, index);
        if (thread == NULL) {
            index++;
            continue;
        }
        cow_list_remove(queue, index);
        thread->status = T_START;
        futex_wake_task(thread);
        woken++;
    }

    if (queue->size == 0) {
        map_remove(futex_map, phys_addr);
        cow_list_destroy(queue);
    }
    return woken;
}

void futex_free(tcb_t thread) {
    if (thread == NULL) { return; }

    for (size_t bucket = 0; bucket < futex_map->capacity; bucket++) {
        map_entry *entry = futex_map->buckets[bucket];
        while (entry) {
            map_entry *next = entry->next;
            void      *key  = entry->key;
            cow_arraylist *queue = (cow_arraylist *)entry->value;
            if (queue != NULL) {
                for (size_t index = 0; index < queue->size;) {
                    tcb_t item = (tcb_t)cow_list_get(queue, index);
                    if (item == thread) {
                        cow_list_remove(queue, index);
                        continue;
                    }
                    index++;
                }
                if (queue->size == 0) {
                    map_remove(futex_map, key);
                    cow_list_destroy(queue);
                }
            }
            entry = next;
        }
    }
}
