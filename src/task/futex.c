#include "task/futex.h"
#include "task/smp.h"
#include "task/eevdf.h"
#include "cow_arraylist.h"
#include "map.h"

map *futex_map = NULL;

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
    wait_eevdf_entity(thread, get_cpu_local(thread->cpu_id));
}

void futex_wake(void *phys_addr, int count) {
    cow_arraylist *queue = (cow_arraylist *)map_get(futex_map, phys_addr);
    if (queue == NULL) { return; }

    tcb_t thread;
    cow_foreach(queue,thread){
        if (thread != NULL) {
            thread->status = T_START;
            futex_eevdf_entity(thread, get_cpu_local(thread->cpu_id));
        }
    }

    if (queue->size == 0) {
        map_remove(futex_map, phys_addr);
        cow_list_destroy(queue);
    }
}

void futex_free(tcb_t thread) {
    if (thread == NULL) { return; }

    for (size_t i = 0; i < futex_map->size; i++) {
        cow_arraylist *queue = (cow_arraylist *)map_get(futex_map, futex_map->buckets[i]->key);
        if (queue != NULL) {
            cow_list_remove(queue, thread->futex_index);
            if (queue->size == 0) {
                map_remove(futex_map, futex_map->buckets[i]->key);
                cow_list_destroy(queue);
            }
        }
    }
}
