/**
 * CP_Kernel 精简版 EEVDF 最小虚拟截止时间优先调度
 */
#pragma GCC push_options
#pragma GCC optimize("O0")

#include "eevdf.h"
#include "cpustats.h"
#include "klog.h"
#include "krlibc.h"

unsigned int sysctl_sched_base_slice = 700000ULL; // 默认时间片长度

const int sched_prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548,  7620,  6100,  4904,  3906,
    /*  -5 */ 3121,  2501,  1991,  1586,  1277,
    /*   0 */ 1024,  820,   655,   526,   423,
    /*   5 */ 335,   272,   215,   172,   137,
    /*  10 */ 110,   87,    70,    56,    45,
    /*  15 */ 36,    29,    23,    18,    15,
};

const uint32_t sched_prio_to_wmult[40] = {
    /* -20 */ 48388,     59856,     76040,     92818,     118348,
    /* -15 */ 147320,    184698,    229616,    287308,    360437,
    /* -10 */ 449829,    563644,    704093,    875809,    1099582,
    /*  -5 */ 1376151,   1717300,   2157191,   2708050,   3363326,
    /*   0 */ 4194304,   5237765,   6557202,   8165337,   10153587,
    /*   5 */ 12820798,  15790321,  19976592,  24970740,  31350126,
    /*  10 */ 39045157,  49367440,  61356676,  76695844,  95443717,
    /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

static uint64_t mul_u64_u32_shr(uint64_t a, uint32_t mul, unsigned int shift) {
    return (uint64_t)(((unsigned __int128)a * mul) >> shift);
}

static inline void avg_vruntime_update(uint64_t delta) {
    eevdf_sched->avg_vruntime -= eevdf_sched->avg_load * delta;
}

static inline int64_t entity_key(struct sched_entity *entity) {
    return (int64_t)(entity->vruntime - eevdf_sched->min_vruntime);
}

static inline bool entity_before(const struct sched_entity *a, const struct sched_entity *b) {
    return (int64_t)(a->deadline - b->deadline) < 0;
}

static int vruntime_eligible(uint64_t vruntime) {
    struct sched_entity *curr = eevdf_sched->current;
    int64_t              avg  = eevdf_sched->avg_vruntime;
    long                 load = eevdf_sched->avg_load;
    if (curr && curr->on_rq) {
        unsigned long weight  = scale_load_down(curr->load.weight);
        avg                  += entity_key(curr) * weight;
        load                 += weight;
    }
    return avg >= (int64_t)(vruntime - eevdf_sched->min_vruntime) * load;
}

void insert_sched_entity(struct rb_root *root, struct sched_entity *se) {
    struct rb_node **link   = &root->rb_node;
    struct rb_node  *parent = NULL;

    while (*link) {
        struct sched_entity *entry;

        parent = *link;
        entry  = container_of(parent, struct sched_entity, run_node);

        if (se->deadline < entry->deadline)
            //if (se->vruntime < entry->vruntime)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }

    rb_link_node(&se->run_node, parent, link);
    rb_insert_color(&se->run_node, root);
}

struct sched_entity *pick_earliest_entity(struct rb_root *root) {
    struct rb_node *node = rb_first(root); // 最小值节点（最左）
    if (!node) return NULL;
    return container_of(node, struct sched_entity, run_node);
}

void set_load_weight(struct sched_entity *entity) {
    int                prio = entity->prio - MAX_RT_PRIO;
    struct load_weight lw;
    if (entity->prio == NICE_TO_PRIO(20)) {
        lw.weight     = scale_load(WEIGHT_IDLEPRIO);
        lw.inv_weight = WMULT_IDLEPRIO;
    } else {
        lw.weight     = scale_load(sched_prio_to_weight[prio]);
        lw.inv_weight = sched_prio_to_wmult[prio];
    }
    entity->load = lw;
}

static void __update_inv_weight(struct load_weight *lw) {
    unsigned long w;

    if (lw->inv_weight) return;

    w = scale_load_down(lw->weight);

    if ((w >= WMULT_CONST) != 0)
        lw->inv_weight = 1;
    else if (!w)
        lw->inv_weight = WMULT_CONST;
    else
        lw->inv_weight = WMULT_CONST / w;
}

static uint64_t __calc_delta(uint64_t delta_exec, unsigned long weight, struct load_weight *lw) {
    uint64_t fact    = scale_load_down(weight);
    uint32_t fact_hi = (uint32_t)(fact >> 32);
    int      shift   = WMULT_SHIFT;
    int      fs;

    __update_inv_weight(lw);

    if (fact_hi) {
        fs      = fls(fact_hi);
        shift  -= fs;
        fact  >>= fs;
    }

    fact = (uint64_t)(fact * lw->inv_weight);

    fact_hi = (uint32_t)(fact >> 32);
    if (fact_hi) {
        fs      = fls(fact_hi);
        shift  -= fs;
        fact  >>= fs;
    }

    return mul_u64_u32_shr(delta_exec, fact, shift);
}

static inline uint64_t calc_delta_fair(uint64_t delta, struct sched_entity *se) {
    if (se->load.weight != NICE_0_LOAD) delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    return delta;
}

static inline uint64_t min_vruntime(uint64_t min_vruntime, uint64_t vruntime) {
    int64_t delta = (int64_t)(vruntime - min_vruntime);
    if (delta < 0) min_vruntime = vruntime;
    return min_vruntime;
}

static bool update_deadline(struct sched_entity *se) {
    if ((int64_t)(se->vruntime - se->deadline) < 0) return false;
    if (!se->custom_slice) se->slice = sysctl_sched_base_slice;
    se->deadline = se->vruntime + calc_delta_fair(se->slice, se);
    return true;
}

struct sched_entity *new_entity(tcb_t task, uint64_t prio, smp_cpu_t *cpu) {
    struct sched_entity *entity = (struct sched_entity *)malloc(sizeof(struct sched_entity));
    entity->is_idle             = prio == NICE_TO_PRIO(20);
    entity->prio                = prio;
    entity->slice               = sysctl_sched_base_slice;
    entity->custom_slice        = 0;
    entity->on_rq               = true;
    entity->deadline            = 0;
    entity->vruntime            = ((struct eevdf_t *)cpu->sched_handle)->min_vruntime;
    entity->exec_start          = sched_clock();
    entity->is_yield            = false;
    set_load_weight(entity);
    update_deadline(entity);
    entity->thread = task;
    return entity;
}

void change_bsp_weight() {
    close_interrupt;
    struct sched_entity *entity = get_current_task()->sched_handle;
    entity->is_idle             = true;
    entity->prio                = NICE_TO_PRIO(20);
    set_load_weight(entity);
    rb_erase(&entity->run_node, eevdf_sched->root);
    insert_sched_entity(eevdf_sched->root, entity);
    open_interrupt;
}

void change_entity_weight(tcb_t thread, uint64_t prio) {
    close_interrupt;
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    if (entity->prio == prio) {
        open_interrupt;
        return;
    }
    entity->is_idle = prio == NICE_TO_PRIO(20);
    entity->prio    = prio;
    set_load_weight(entity);
    rb_erase(&entity->run_node, eevdf_sched->root);
    insert_sched_entity(eevdf_sched->root, entity);
    open_interrupt;
}

struct sched_entity *pick_eevdf() {
    struct sched_entity *se   = pick_earliest_entity(eevdf_sched->root);
    struct sched_entity *curr = eevdf_sched->current;
    struct sched_entity *best = NULL;
    struct rb_node      *node = eevdf_sched->root->rb_node;

    if (se && vruntime_eligible(se->vruntime)) {
        best = se;
        goto found;
    }

    while (node) {
        struct rb_node *left = node->rb_left;
        if (left &&
            vruntime_eligible(container_of(left, struct sched_entity, run_node)->min_vruntime)) {
            node = left;
            continue;
        }
        se = container_of(node, struct sched_entity, run_node);
        if (vruntime_eligible(se->vruntime)) {
            best = se;
            break;
        }
        node = node->rb_right;
    }

found:;
    if (!best || (curr && entity_before(curr, best))) best = curr;

    return best;
}

static uint64_t __update_min_vruntime(uint64_t vruntime) {
    uint64_t min_vruntime = eevdf_sched->min_vruntime;
    int64_t  delta        = (int64_t)(vruntime - min_vruntime);
    if (delta > 0) {
        avg_vruntime_update(delta);
        min_vruntime = vruntime;
    }
    return min_vruntime;
}

static void update_min_vruntime() {
    struct sched_entity *se =
        container_of(eevdf_sched->root->rb_node, struct sched_entity, run_node);
    struct sched_entity *curr     = eevdf_sched->current;
    uint64_t             vruntime = eevdf_sched->min_vruntime;

    if (curr) {
        if (curr->on_rq)
            vruntime = curr->vruntime;
        else
            curr = NULL;
    }

    if (se) {
        if (!curr)
            vruntime = se->min_vruntime;
        else
            vruntime = min_vruntime(vruntime, se->vruntime);
    }
    eevdf_sched->min_vruntime = MAX(__update_min_vruntime(vruntime), vruntime);
}

static int64_t update_curr_se(struct sched_entity *curr) {
    uint64_t now = sched_clock();
    int64_t  delta_exec;

    delta_exec = now - curr->exec_start;
    if (delta_exec <= 0) return delta_exec;

    curr->exec_start        = now;
    curr->sum_exec_runtime += delta_exec;
    return delta_exec;
}

void update_current_task() {
    struct sched_entity *curr = eevdf_sched->current;
    if (!curr) return;
    bool resche;
    if (curr->is_yield) {
        curr->vruntime = curr->deadline;
        resche         = update_deadline(curr);
        curr->is_yield = false;
        goto resche;
    }
    int64_t delta_exec;
    delta_exec = update_curr_se(curr);
    if (delta_exec <= 0) return;
    curr->vruntime += calc_delta_fair(delta_exec, curr);
    resche          = update_deadline(curr);
    update_min_vruntime();

resche:;
    if (resche) {
        rb_erase(&curr->run_node, eevdf_sched->root);
        insert_sched_entity(eevdf_sched->root, curr);
    }
}

void remove_sched_entity(struct rb_root *root, struct sched_entity *se) {
    rb_erase(&se->run_node, root);
    struct sched_entity *current = eevdf_sched->current;
    if (current == se) eevdf_sched->current = NULL;
    eevdf_sched->current = pick_eevdf();
    update_min_vruntime();
}

tcb_t pick_next_task() {
    update_current_task();
    struct sched_entity *current = pick_eevdf();
    current->exec_start          = sched_clock();
    eevdf_sched->current         = current;
    return current->thread;
}

void add_eevdf_entity_with_prio(tcb_t new_task, uint64_t prio, smp_cpu_t *cpu) {
    struct sched_entity *entity = new_entity(new_task, prio, cpu);
    entity->handle              = cpu->sched_handle;
    new_task->sched_handle      = entity;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched->task_count++;
}

void add_eevdf_entity(tcb_t new_task, smp_cpu_t *cpu) {
    struct sched_entity *entity = new_entity(new_task, NICE_TO_PRIO(0), cpu);
    new_task->sched_handle      = entity;
    entity->handle              = cpu->sched_handle;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched->task_count++;
}

void remove_eevdf_entity(tcb_t thread, smp_cpu_t *cpu) {
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    remove_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    free(entity);
    eevdf_sched->task_count--;
}

void wait_eevdf_entity(tcb_t thread, smp_cpu_t *cpu) {
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    remove_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    entity->wait_index = queue_enqueue(((struct eevdf_t *)cpu->sched_handle)->wait_queue, entity);
    eevdf_sched->task_count--;
}

void futex_eevdf_entity(tcb_t thread, smp_cpu_t *cpu) {
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    queue_remove_at(((struct eevdf_t *)cpu->sched_handle)->wait_queue, entity->wait_index);
    entity->handle = cpu->sched_handle;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched->task_count++;
}

void init_ap_idle(smp_cpu_t *cpu, tcb_t ap_idle) {
    cpu->sched_handle = (struct eevdf_t *)calloc(1, sizeof(struct eevdf_t));
    ((struct eevdf_t *)cpu->sched_handle)->root =
        (struct rb_root *)calloc(1, sizeof(struct rb_root));
    ((struct eevdf_t *)cpu->sched_handle)->min_vruntime = 0;
    ((struct eevdf_t *)cpu->sched_handle)->wait_queue   = queue_init();
    ((struct eevdf_t *)cpu->sched_handle)->task_count   = 0;
    struct sched_entity *idle_entity                   = new_entity(ap_idle, NICE_TO_PRIO(20), cpu);
    ((struct eevdf_t *)cpu->sched_handle)->idle_entity = idle_entity;
    ap_idle->sched_handle                              = idle_entity;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, idle_entity);
    eevdf_sched->current = idle_entity;
}

void cb(struct rb_node *node) {
    struct sched_entity *entity = container_of(node, struct sched_entity, run_node);
    tcb_t                thread = entity->thread;
    logkf("name: %s, tid: %d, cmdline: %s, deadline: %lu, vruntime: %lu\r\n", thread->name,
          thread->tid, thread->parent_group->cmdline, entity->deadline, entity->vruntime);
}

USED void for_each_rb_tree() {
    rb_traverse_with_callback(((struct eevdf_t *)current_cpu->sched_handle)->root, cb);
    logkf("\r\n");
}

#pragma GCC pop_options
