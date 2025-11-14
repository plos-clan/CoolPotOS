#include "muqss.h"
#include "cpustats.h"

/* Pre-calculated weight table for each nice value (-20 to +19) */
static const int sched_prio_to_weight[NICE_WIDTH] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548,  7620,  6100,  4904,  3906,
    /*  -5 */ 3121,  2501,  1991,  1586,  1277,
    /*   0 */ 1024,  820,   655,   526,   423,
    /*   5 */ 335,   272,   215,   172,   137,
    /*  10 */ 110,   87,    70,    56,    45,
    /*  15 */ 36,    29,    23,    18,    15,
};

/* Pre-calculated inverse weight (for division optimization) */
static const uint32_t sched_prio_to_wmult[NICE_WIDTH] = {
    /* -20 */ 48388,     59856,     76040,     92818,     118348,
    /* -15 */ 147320,    184698,    229616,    287308,    360437,
    /* -10 */ 449829,    563644,    704093,    875809,    1099582,
    /*  -5 */ 1376151,   1717300,   2157191,   2708050,   3363326,
    /*   0 */ 4194304,   5237765,   6557202,   8165337,   10153587,
    /*   5 */ 12820798,  15790321,  19976592,  24970740,  31350126,
    /*  10 */ 39045157,  49367440,  61356676,  76695844,  95443717,
    /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

/* Get weight with bounds checking */
static inline int get_weight_for_nice(int nice) {
    if (!NICE_IN_RANGE(nice)) nice = CLAMP_NICE(nice);
    return NICE_TO_WEIGHT(nice);
}

/* Calculate weighted virtual runtime delta */
static inline uint64_t calc_vruntime_delta(uint64_t delta_exec, int weight) {
    /* vruntime_delta = (delta_exec * NICE_0_LOAD) / task_weight */
    return (delta_exec * NICE_0_LOAD) / weight;
}

/* Calculate time slice based on nice value */
static inline uint64_t calc_timeslice(int nice) {
    return NICE_TO_TIMESLICE(nice);
}

/* Random level generation for skip list */
static int random_level(void) {
    int level = 1;
    while ((sched_clock() & 0xFFFFFFFF) < UINT32_MAX / 2 && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

/* Initialize scheduling entity */
struct sched_entity *sched_entity_create(tcb_t task, uint64_t priority) {
    struct sched_entity *se = malloc(sizeof(struct sched_entity));
    if (!se) return NULL;

    memset(se, 0, sizeof(struct sched_entity));
    se->task       = task;
    se->vruntime   = 0;
    se->deadline   = 0;
    se->time_slice = calc_timeslice(PRIO_TO_NICE(priority));
    se->weight     = get_weight_for_nice(PRIO_TO_NICE(priority));
    se->is_yield   = false;
    se->node       = NULL;
    se->rq         = NULL;
    se->nice       = PRIO_TO_NICE(priority);

    return se;
}

/* Calculate virtual deadline based on weight */
static uint64_t calc_deadline(struct sched_entity *se, uint64_t now) {
    tcb_t task = se->task;

    /* Deadline calculation based on weight */
    /* Lower weight (higher nice) = later deadline */
    /* Higher weight (lower nice) = earlier deadline */

    uint64_t weighted_slice = (se->time_slice * NICE_0_LOAD) / se->weight;
    return se->vruntime + weighted_slice;
}

static struct skiplist_node *skiplist_create_node(int level, struct sched_entity *se) {
    struct skiplist_node *node = malloc(sizeof(struct skiplist_node));
    if (!node) return NULL;

    node->se    = se;
    node->level = level;
    for (int i = 0; i < MAX_LEVEL; i++) {
        node->forward[i] = NULL;
    }

    return node;
}

static inline int se_compare(struct sched_entity *se1, struct sched_entity *se2) {
    if (!se1 || !se2) return 0;

    /* First compare by deadline */
    if (se1->deadline < se2->deadline) return -1;
    if (se1->deadline > se2->deadline) return 1;

    /* If deadlines are equal, compare by vruntime */
    if (se1->vruntime < se2->vruntime) return -1;
    if (se1->vruntime > se2->vruntime) return 1;

    /* If both are equal, compare by PID for consistency */
    if (se1->task->tid < se2->task->tid) return -1;
    if (se1->task->tid > se2->task->tid) return 1;

    return 0;
}

/* Insert entity into skip list (ordered by deadline, then vruntime) */
static void skiplist_insert(struct rq *rq, struct sched_entity *se) {
    if (!rq || !se) return;

    struct skiplist_node *update[MAX_LEVEL];
    struct skiplist_node *x = rq->head;

    /* Initialize update array */
    for (int i = 0; i < MAX_LEVEL; i++) {
        update[i] = NULL;
    }

    /* Find position for insertion */
    for (int i = rq->max_level - 1; i >= 0; i--) {
        /* Traverse at current level */
        while (x->forward[i] != NULL && se_compare(x->forward[i]->se, se) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    /* Generate random level for new node */
    int new_level = random_level();

    /* If new level is greater than current max level, update */
    if (new_level > rq->max_level) {
        for (int i = rq->max_level; i < new_level; i++) {
            update[i] = rq->head;
        }
        rq->max_level = new_level;
    }

    /* Create and insert new node */
    struct skiplist_node *new_node = skiplist_create_node(new_level, se);
    if (!new_node) return;

    se->node = new_node;

    /* Update forward pointers */
    for (int i = 0; i < new_level; i++) {
        if (update[i]) {
            new_node->forward[i]  = update[i]->forward[i];
            update[i]->forward[i] = new_node;
        }
    }

    rq->nr_running++;
}

/* Delete entity from skip list */
static void skiplist_delete(struct rq *rq, struct sched_entity *se) {
    if (!rq || !se || !se->node) return;

    struct skiplist_node *update[MAX_LEVEL];
    struct skiplist_node *x = rq->head;

    /* Initialize update array */
    for (int i = 0; i < MAX_LEVEL; i++) {
        update[i] = NULL;
    }

    /* Find the node to delete */
    for (int i = rq->max_level - 1; i >= 0; i--) {
        while (x->forward[i] != NULL && se_compare(x->forward[i]->se, se) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    /* Move to the node that should be deleted */
    x = (x->forward[0] != NULL) ? x->forward[0] : NULL;

    /* Verify this is the correct node */
    if (x == NULL || x->se != se) {
        /* Node not found in expected position, do exhaustive search */
        struct skiplist_node *current = rq->head->forward[0];
        struct skiplist_node *prev    = rq->head;

        while (current != NULL) {
            if (current->se == se) {
                /* Found it, rebuild update array */
                x = current;
                for (int i = rq->max_level - 1; i >= 0; i--) {
                    struct skiplist_node *p = rq->head;
                    while (p->forward[i] != NULL && p->forward[i] != x) {
                        p = p->forward[i];
                    }
                    update[i] = p;
                }
                break;
            }
            prev    = current;
            current = current->forward[0];
        }

        if (x == NULL || x->se != se) {
            /* Still not found, nothing to delete */
            return;
        }
    }

    /* Remove node from all levels */
    for (int i = 0; i < rq->max_level; i++) {
        if (update[i] && update[i]->forward[i] == x) { update[i]->forward[i] = x->forward[i]; }
    }

    /* Update max level if necessary */
    while (rq->max_level > 1 && rq->head->forward[rq->max_level - 1] == NULL) {
        rq->max_level--;
    }

    /* Free the node */
    free(x);
    se->node = NULL;
    rq->nr_running--;
}

/* Peek at first ready task */
static struct sched_entity *skiplist_peek_first(struct rq *rq) {
    struct skiplist_node *node = rq->head->forward[0];
    while (node != NULL) {
        if (node->se->task->status == START) { return node->se; }
        node = node->forward[0];
    }
    return NULL;
}

/* Find next ready task that's not the current task */
static struct sched_entity *skiplist_find_next_ready(struct rq *rq, tcb_t curr) {
    struct skiplist_node *node = rq->head->forward[0];

    while (node != NULL) {
        struct sched_entity *se = node->se;
        /* Skip current task and non-ready tasks */
        if (se->task != curr && se->task->status == START) { return se; }
        node = node->forward[0];
    }

    return NULL;
}

/* Update minimum vruntime */
static void update_min_vruntime(struct rq *rq) {
    uint64_t min_vruntime = rq->min_vruntime;

    /* Check current task */
    if (rq->curr) {
        struct sched_entity *se = (struct sched_entity *)rq->curr->sched_handle;
        if (se) { min_vruntime = se->vruntime; }
    }

    /* Check leftmost task in queue */
    struct sched_entity *leftmost = skiplist_peek_first(rq);
    if (leftmost) {
        if (!rq->curr) {
            min_vruntime = leftmost->vruntime;
        } else {
            struct sched_entity *curr_se = (struct sched_entity *)rq->curr->sched_handle;
            min_vruntime =
                (curr_se->vruntime < leftmost->vruntime) ? curr_se->vruntime : leftmost->vruntime;
        }
    }

    rq->min_vruntime = min_vruntime;
}

/* ====================================================================
 * Run Queue Operations
 * ==================================================================== */

/* Initialize run queue */
void rq_init(struct rq *rq) {
    memset(rq, 0, sizeof(struct rq));

    /* Create head node with NULL sched_entity */
    rq->head         = skiplist_create_node(MAX_LEVEL, NULL);
    rq->max_level    = 1;
    rq->nr_running   = 0;
    rq->clock        = sched_clock();
    rq->curr         = NULL;
    rq->min_vruntime = 0;
}

/* Enqueue task into run queue */
void enqueue_task(struct rq *rq, tcb_t task) {
    struct sched_entity *se = (struct sched_entity *)task->sched_handle;
    if (!se) return;

    if (se->rq) return;

    /* Set vruntime for new task to min_vruntime */
    if (se->vruntime < rq->min_vruntime) { se->vruntime = rq->min_vruntime; }

    /* Calculate deadline */
    se->deadline   = calc_deadline(se, rq->clock);
    se->rq         = rq;
    se->time_slice = calc_timeslice(se->nice);

    /* Insert into skiplist */
    skiplist_insert(rq, se);

    update_min_vruntime(rq);
}

/* Dequeue task from run queue */
void dequeue_task(struct rq *rq, tcb_t task) {
    struct sched_entity *se = (struct sched_entity *)task->sched_handle;
    if (!se || !se->node || !se->rq) return;

    skiplist_delete(rq, se);
    se->rq = NULL;

    update_min_vruntime(rq);
}

/* Update current task's runtime */
static void update_curr(struct rq *rq, uint64_t now) {
    if (!rq->curr) return;

    struct sched_entity *se = (struct sched_entity *)rq->curr->sched_handle;
    if (!se || se->exec_start == 0) return;

    uint64_t delta        = now - se->exec_start;
    se->sum_exec_runtime += delta;

    /* Calculate weighted virtual runtime */
    uint64_t vruntime_delta  = calc_vruntime_delta(delta, se->weight);
    se->vruntime            += vruntime_delta;

    if (se->time_slice > delta) {
        se->time_slice -= delta;
    } else {
        se->time_slice = 0;
    }

    se->exec_start = now;

    update_min_vruntime(rq);
}

/* Pick next task to run */
tcb_t muqss_pick_next_task(struct rq *rq) {
    struct sched_entity *se = skiplist_peek_first(rq);
    if (!se) return NULL;

    /* Start tracking execution time */
    se->exec_start = rq->clock;
    se->time_slice = calc_timeslice(se->nice);

    return se->task;
}

/* Handle yield in schedule function */
static void handle_yield(struct rq *rq) {
    if (!rq || !rq->curr) return;

    tcb_t                yielding_task = rq->curr;
    struct sched_entity *se            = (struct sched_entity *)yielding_task->sched_handle;

    if (!se) {
        se->is_yield = false;
        return;
    }

    /* Update runtime */
    update_curr(rq, rq->clock);

    /* Dequeue the yielding task */
    dequeue_task(rq, yielding_task);

    /* Apply yield penalty if task is still ready */
    if (yielding_task->status == START) {
        /* Penalize by half time slice worth of vruntime */
        uint64_t penalty  = calc_vruntime_delta(se->time_slice / 2, se->weight);
        se->vruntime     += penalty;

        /* Re-enqueue with updated vruntime */
        enqueue_task(rq, yielding_task);
    }

    /* Clear yield flag */
    se->is_yield = false;

    /* Try to find a different ready task */
    struct sched_entity *next_se = skiplist_find_next_ready(rq, yielding_task);

    tcb_t next;
    if (next_se && next_se->task != yielding_task) {
        /* Found a different ready task */
        next = next_se->task;
    } else {
        /* No other task, pick whatever is best (may be same task or idle) */
        next = muqss_pick_next_task(rq);
    }

    if (rq->curr == next) {
        next = NULL; //TODO
    }

    /* Switch to next task */
    rq->curr = next;
    rq->nr_switches++;

    /* Start execution time tracking for new task */
    struct sched_entity *next_se_final = (struct sched_entity *)next->sched_handle;
    if (next_se_final) { next_se_final->exec_start = rq->clock; }
}

/* Schedule - main scheduling decision */
void schedule(struct rq *rq) {
    if (!rq) return;

    rq->clock = sched_clock();

    /* rq->curr should never be NULL, but check just in case */
    if (!rq->curr) { rq->curr = rq->idle; }

    struct sched_entity *curr_se = (struct sched_entity *)rq->curr->sched_handle;

    if (!curr_se) {
        rq->curr = rq->idle;
        return;
    }

    /* Handle yield first if set */
    if (curr_se->is_yield) {
        handle_yield(rq);
        return; /* Yield already picked next task */
    }

    /* Update current task */
    update_curr(rq, rq->clock);

    /* Check if time slice expired or task is no longer ready */
    bool need_resched = false;

    if (rq->curr == rq->idle) {
        /* Idle task always yields if there are ready tasks */
        struct sched_entity *next = skiplist_peek_first(rq);
        if (next != NULL) { need_resched = true; }
    } else if (curr_se->time_slice == 0 || rq->curr->status != START) {
        need_resched = true;

        /* Dequeue if not ready */
        if (rq->curr->status != START) {
            dequeue_task(rq, rq->curr);
        } else {
            /* Re-enqueue with new deadline */
            dequeue_task(rq, rq->curr);
            enqueue_task(rq, rq->curr);
        }
    }

    /* Pick next task if needed */
    if (need_resched) {
        tcb_t prev = rq->curr;
        rq->curr   = muqss_pick_next_task(rq);

        if (rq->curr != prev) { rq->nr_switches++; }
    }
}
