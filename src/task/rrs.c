#include "task/rrs.h"
#include "task/scheduler.h"

void add_rrs_entity(const tcb_t thread, const cpu_local_t *local) {
    const rrs_t *scheduler      = local->sched_handle;
    struct sched_entity *entity = malloc(sizeof(struct sched_entity));
    asserts(entity, "add_rrs_entity: entity is null.");
    entity->thread       = thread;
    entity->node         = list_enqueue(scheduler->sched_queue, entity);
    thread->sched_handle = entity;
}

void remove_rrs_entity(const tcb_t thread, const cpu_local_t *local) {
    if (thread == NULL || local == NULL || thread->sched_handle == NULL) {
        return;
    }
    rrs_t *scheduler            = local->sched_handle;
    struct sched_entity *entity = thread->sched_handle;
    list_remove_node(scheduler->sched_queue, entity->node);
    if (scheduler->curr == entity) {
        scheduler->curr = scheduler->idle;
    }
    free(entity);
    thread->sched_handle = NULL;
}

tcb_t rrs_pick_next_task(const cpu_local_t *local) {
    rrs_t *scheduler = local->sched_handle;

    if (scheduler->sched_queue->size == 1) {
        return scheduler->idle->thread;
    }

resche:;
    const struct sched_entity *entity = scheduler->curr;
    const list_node_t *nextL          = entity->node->next;
    struct sched_entity *next;
    if (nextL == NULL) {
        next = scheduler->idle;
    } else {
        next = nextL->data;
    }
    scheduler->curr = next;
    if (scheduler->curr == scheduler->idle) {
        goto resche;
    }
    return next->thread;
}

void init_cpu_idle_rrs(cpu_local_t *local, const tcb_t idle) {
    rrs_t *scheduler = calloc(1, sizeof(rrs_t));
    asserts(scheduler, "init_cpu_idle_rrs: scheduler is null.");
    local->sched_handle           = scheduler;
    scheduler->sched_queue      = create_llist_queue();
    struct sched_entity *entity = malloc(sizeof(struct sched_entity));
    asserts(entity, "init_cpu_idle_rrs: entity is null.");
    entity->thread  = idle;
    entity->node    = list_enqueue(scheduler->sched_queue, entity);
    scheduler->idle = entity;
    scheduler->curr = entity;
}
