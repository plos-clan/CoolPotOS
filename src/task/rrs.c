#include "task/rrs.h"
#include "task/scheduler.h"

void add_rrs_entity(tcb_t thread, cpu_local_t *local) {
    rrs_t               *scheduler = local->sched_handle;
    struct sched_entity *entity    = malloc(sizeof(struct sched_entity));
    entity->thread                 = thread;
    entity->node                   = list_enqueue(scheduler->sched_queue, entity);
    thread->sched_handle           = entity;
}

void remove_rrs_entity(tcb_t thread, cpu_local_t *local) {
    rrs_t               *scheduler = local->sched_handle;
    struct sched_entity *entity    = thread->sched_handle;
    list_remove_node(scheduler->sched_queue, entity->node);
    if (scheduler->curr == entity) scheduler->curr = scheduler->idle;
    free(entity);
}

tcb_t rrs_pick_next_task(cpu_local_t *local) {
    rrs_t *scheduler = local->sched_handle;

    if (scheduler->sched_queue->size == 1) { return scheduler->idle->thread; }

resche:;
    struct sched_entity *entity = scheduler->curr;
    list_node_t         *nextL  = entity->node->next;
    struct sched_entity *next;
    if (nextL == NULL)
        next = scheduler->idle;
    else {
        next = nextL->data;
    }
    scheduler->curr = next;
    if (scheduler->curr == scheduler->idle) { goto resche; }
    return next->thread;
}

void init_cpu_idle_rrs(cpu_local_t *cpu, tcb_t idle) {
    rrs_t *scheduler            = (rrs_t *)calloc(1, sizeof(rrs_t));
    cpu->sched_handle           = scheduler;
    scheduler->sched_queue      = create_llist_queue();
    struct sched_entity *entity = malloc(sizeof(struct sched_entity));
    entity->thread              = idle;
    entity->node                = list_enqueue(scheduler->sched_queue, entity);
    scheduler->idle             = entity;
    scheduler->curr             = entity;
}
