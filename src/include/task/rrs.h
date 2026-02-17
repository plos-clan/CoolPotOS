#pragma once

#include "llist_queue.h"
#include "task/smp.h"
#include "task/task.h"

struct sched_entity {
    tcb_t        thread;
    list_node_t *node;
};

typedef struct rrs_scheduler {
    list_queue_t        *sched_queue;
    struct sched_entity *idle;
    struct sched_entity *curr;
} rrs_t;

void  add_rrs_entity(tcb_t thread, cpu_local_t *local);
void  remove_rrs_entity(tcb_t thread, cpu_local_t *local);
tcb_t rrs_pick_next_task(cpu_local_t *local);
void  init_cpu_idle_rrs(cpu_local_t *local, tcb_t idle);
