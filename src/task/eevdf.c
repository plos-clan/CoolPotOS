/**
 * CP_Kernel 精简版 EEVDF 最小虚拟截止时间优先调度
 */
// #pragma GCC push_options
// #pragma GCC optimize("O0")

#include "task/eevdf.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "task/scheduler.h"
#include "task/smp.h"
#include "term/klog.h"
#include "timer.h"

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

static inline void avg_vruntime_update(uint64_t delta, cpu_local_t *cpu) {
    eevdf_sched(cpu)->avg_vruntime -= eevdf_sched(cpu)->avg_load * delta;
}

static inline int64_t entity_key(struct sched_entity *entity, cpu_local_t *cpu) {
    return (int64_t)(entity->vruntime - eevdf_sched(cpu)->min_vruntime);
}

static inline bool entity_before(const struct sched_entity *a, const struct sched_entity *b) {
    return (int64_t)(a->deadline - b->deadline) < 0;
}

static int vruntime_eligible(uint64_t vruntime, cpu_local_t *cpu) {
    struct sched_entity *curr = eevdf_sched(cpu)->current;
    int64_t avg = eevdf_sched(cpu)->avg_vruntime;
    long load = eevdf_sched(cpu)->avg_load;
    if (unlikely(load == 0))
        return 1;
    if (curr && curr->on_rq) {
        unsigned long weight = scale_load_down(curr->load.weight);
        avg += entity_key(curr, cpu) * weight;
        load += weight;
    }
    return avg >= (int64_t)(vruntime - eevdf_sched(cpu)->min_vruntime) * load;
}

void insert_sched_entity(struct rb_root *root, struct sched_entity *se) {
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*link) {
        struct sched_entity *entry;

        parent = *link;
        entry = container_of(parent, struct sched_entity, run_node);

        if (se->deadline < entry->deadline)
            // if (se->vruntime < entry->vruntime)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }

    rb_link_node(&se->run_node, parent, link);
    rb_insert_color(&se->run_node, root);
}

struct sched_entity *pick_earliest_entity(struct rb_root *root) {
    struct rb_node *node = rb_first(root); // 最小值节点（最左）
    if (!node)
        return NULL;
    return container_of(node, struct sched_entity, run_node);
}

void set_load_weight(struct sched_entity *entity) {
    int prio = entity->prio - MAX_RT_PRIO;
    struct load_weight lw;
    if (entity->prio == NICE_TO_PRIO(20)) {
        lw.weight = scale_load(WEIGHT_IDLEPRIO);
        lw.inv_weight = WMULT_IDLEPRIO;
    } else {
        lw.weight = scale_load(sched_prio_to_weight[prio]);
        lw.inv_weight = sched_prio_to_wmult[prio];
    }
    entity->load = lw;
}

static void __update_inv_weight(struct load_weight *lw) {
    unsigned long w;

    if (lw->inv_weight)
        return;

    w = scale_load_down(lw->weight);

    if ((w >= WMULT_CONST) != 0)
        lw->inv_weight = 1;
    else if (!w)
        lw->inv_weight = WMULT_CONST;
    else
        lw->inv_weight = WMULT_CONST / w;
}

static inline void __min_vruntime_update(struct sched_entity *se, struct rb_node *node) {
    if (node) {
        struct sched_entity *rse = __node_2_se(node);
        if (vruntime_gt(min_vruntime, se, rse))
            se->min_vruntime = rse->min_vruntime;
    }
}

static uint64_t __calc_delta(uint64_t delta_exec, unsigned long weight, struct load_weight *lw) {
    uint64_t fact = scale_load_down(weight);
    uint32_t fact_hi = (uint32_t)(fact >> 32);
    int shift = WMULT_SHIFT;
    int fs;

    __update_inv_weight(lw);

    if (fact_hi) {
        fs = fls(fact_hi);
        shift -= fs;
        fact >>= fs;
    }

    fact = (uint64_t)(fact * lw->inv_weight);

    fact_hi = (uint32_t)(fact >> 32);
    if (fact_hi) {
        fs = fls(fact_hi);
        shift -= fs;
        fact >>= fs;
    }

    return mul_u64_u32_shr(delta_exec, fact, shift);
}

static inline bool min_vruntime_update(struct sched_entity *se, bool exit) {
    uint64_t old_min_vruntime = se->min_vruntime;
    struct rb_node *node = &se->run_node;

    se->min_vruntime = se->vruntime;
    __min_vruntime_update(se, node->rb_right);
    __min_vruntime_update(se, node->rb_left);

    return se->min_vruntime == old_min_vruntime;
}

static inline uint64_t calc_delta_fair(uint64_t delta, struct sched_entity *se) {
    if (se->load.weight != NICE_0_LOAD)
        delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    return delta;
}

static inline uint64_t min_vruntime(uint64_t min_vruntime, uint64_t vruntime) {
    int64_t delta = (int64_t)(vruntime - min_vruntime);
    if (delta < 0)
        min_vruntime = vruntime;
    return min_vruntime;
}

static bool update_deadline(struct sched_entity *se) {
    if ((int64_t)(se->vruntime - se->deadline) < 0)
        return false;
    if (!se->custom_slice)
        se->slice = sysctl_sched_base_slice;
    se->deadline = se->vruntime + calc_delta_fair(se->slice, se);
    return true;
}

struct sched_entity *new_entity(tcb_t task, uint64_t prio, cpu_local_t *cpu) {
    struct sched_entity *entity = (struct sched_entity *)malloc(sizeof(struct sched_entity));
    entity->is_idle = prio == NICE_TO_PRIO(20);
    entity->prio = prio;
    entity->slice = sysctl_sched_base_slice;
    entity->custom_slice = 0;
    entity->on_rq = true;
    entity->deadline = 0;
    entity->vruntime = ((struct eevdf_t *)cpu->sched_handle)->min_vruntime;
    entity->min_vruntime = entity->vruntime;
    entity->exec_start = sched_clock();
    entity->is_yield = false;
    set_load_weight(entity);
    update_deadline(entity);
    entity->thread = task;
    return entity;
}

void get_all_eevdf() {
    eevdf_t *eevdf = eevdf_sched(arch_current_cpu());
    logkf("all min_vruntime:%lx", eevdf->min_vruntime);
}

void change_entity_weight(tcb_t thread, uint64_t prio, cpu_local_t *cpu) {
    arch_close_interrupt();
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    if (entity->prio == prio) {
        arch_open_interrupt();
        return;
    }
    entity->is_idle = prio == NICE_TO_PRIO(20);
    entity->prio = prio;
    set_load_weight(entity);
    rb_erase(&entity->run_node, eevdf_sched(cpu)->root);
    insert_sched_entity(eevdf_sched(cpu)->root, entity);
    arch_open_interrupt();
}

struct sched_entity *pick_eevdf(cpu_local_t *cpu) {
    struct sched_entity *se = pick_earliest_entity(eevdf_sched(cpu)->root);
    struct sched_entity *curr = eevdf_sched(cpu)->current;
    struct sched_entity *best = NULL;
    struct rb_node *node = eevdf_sched(cpu)->root->rb_node;

    if (se && vruntime_eligible(se->vruntime, cpu)) {
        best = se;
        goto found;
    }

    while (node) {
        struct rb_node *left = node->rb_left;
        if (left
            && vruntime_eligible(
                container_of(left, struct sched_entity, run_node)->min_vruntime, cpu)) {
            node = left;
            continue;
        }
        se = container_of(node, struct sched_entity, run_node);
        if (vruntime_eligible(se->vruntime, cpu)) {
            best = se;
            break;
        }
        node = node->rb_right;
    }

found:;
    if (!best || (curr && entity_before(curr, best)))
        best = curr;

    return best;
}

static uint64_t __update_min_vruntime(uint64_t vruntime, cpu_local_t *cpu) {
    uint64_t min_vruntime = eevdf_sched(cpu)->min_vruntime;
    int64_t delta = (int64_t)(vruntime - min_vruntime);
    if (delta > 0) {
        avg_vruntime_update(delta, cpu);
        min_vruntime = vruntime;
    }
    return min_vruntime;
}

static void update_min_vruntime(cpu_local_t *cpu) {
    struct rb_node *node = eevdf_sched(cpu)->root ? eevdf_sched(cpu)->root->rb_node : NULL;
    struct sched_entity *se = node ? container_of(node, struct sched_entity, run_node) : NULL;
    struct sched_entity *curr = eevdf_sched(cpu)->current;
    uint64_t vruntime = eevdf_sched(cpu)->min_vruntime;

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
    eevdf_sched(cpu)->min_vruntime = MAX(__update_min_vruntime(vruntime, cpu), vruntime);
}

// 溢出检查
static void wrap_vruntime(cpu_local_t *cpu) {
    struct eevdf_t *eevdf = eevdf_sched(cpu);
    if (likely(eevdf->min_vruntime < VRUNTIME_OFFSET_THRESHOLD))
        return;
    uint64_t offset = VRUNTIME_OFFSET_THRESHOLD;
    eevdf->min_vruntime -= offset;
    struct sched_entity *curr = eevdf->current;
    if (curr) {
        curr->vruntime -= offset;
        curr->deadline -= offset;
    }
    struct rb_node *node;
    for (node = rb_first(eevdf->root); node; node = rb_next(node)) {
        struct sched_entity *se = container_of(node, struct sched_entity, run_node);
        if (se->vruntime > offset)
            se->vruntime -= offset;
        if (se->deadline > offset)
            se->deadline -= offset;
    }
    if (eevdf->avg_load) {
        eevdf->avg_vruntime -= (uint64_t)eevdf->avg_load * offset;
    } else {
        eevdf->avg_vruntime = 0;
    }
}

static int64_t update_curr_se(struct sched_entity *curr) {
    uint64_t now = sched_clock();
    int64_t delta_exec;

    delta_exec = now - curr->exec_start;
    if (unlikely(delta_exec <= 0))
        return delta_exec;

    curr->exec_start = now;
    curr->sum_exec_runtime += delta_exec;
    return delta_exec;
}

static void update_vlag(struct sched_entity *se, cpu_local_t *cpu) {
    int64_t vlag_raw = 0;
    int64_t limit = 0;
    vlag_raw = eevdf_sched(cpu)->min_vruntime - se->vruntime;
    limit = calc_delta_fair(MAX(2 * se->slice, TICK_NSEC), se);
    se->vlag = clamp(vlag_raw, -limit, limit);
}

void update_current_task(cpu_local_t *cpu) {
    struct sched_entity *curr = eevdf_sched(cpu)->current;
    if (unlikely(!curr))
        return;
    bool resche;
    int64_t delta_exec;
    delta_exec = update_curr_se(curr);
    if (unlikely(delta_exec <= 0)) {
        if (curr->is_yield) {
            curr->deadline = curr->vruntime + calc_delta_fair(curr->slice, curr);
            curr->is_yield = false;
            min_vruntime_update(curr, false);
            rb_erase(&curr->run_node, eevdf_sched(cpu)->root);
            insert_sched_entity(eevdf_sched(cpu)->root, curr);
        }
        return;
    }
    curr->vruntime += calc_delta_fair(delta_exec, curr);
    update_vlag(curr, cpu);
    resche = update_deadline(curr);
    update_min_vruntime(cpu);
    curr->min_vruntime = eevdf_sched(cpu)->min_vruntime;
    if (curr->is_yield) {
        curr->deadline = curr->vruntime + calc_delta_fair(curr->slice, curr);
        curr->is_yield = false;
        resche = true;
    }
    if (resche || curr->is_idle) {
        min_vruntime_update(curr, false);
        rb_erase(&curr->run_node, eevdf_sched(cpu)->root);
        insert_sched_entity(eevdf_sched(cpu)->root, curr);
    }
    wrap_vruntime(cpu);
}

void set_entity_yield(tcb_t thread) {
    struct sched_entity *entity = thread->sched_handle;
    entity->is_yield = true;
}

void remove_sched_entity(struct rb_root *root, struct sched_entity *se, cpu_local_t *cpu) {
    rb_erase(&se->run_node, root);
    se->on_rq = false;
    struct sched_entity *current = eevdf_sched(cpu)->current;
    if (current == se)
        eevdf_sched(cpu)->current = NULL;
    eevdf_sched(cpu)->current = pick_eevdf(cpu);
    min_vruntime_update(se, true); // 更新子树节点 min_vruntime
    update_min_vruntime(cpu);      // 更新全局 min_vruntime
}

tcb_t eevdf_pick_next_task(cpu_local_t *cpu) {
    update_current_task(cpu);
    struct sched_entity *current = pick_eevdf(cpu);
    current->exec_start = sched_clock();
    eevdf_sched(cpu)->current = current;
    return current->thread;
}

void add_eevdf_entity_with_prio(tcb_t new_task, uint64_t prio, cpu_local_t *cpu) {
    struct sched_entity *entity = new_entity(new_task, prio, cpu);
    entity->handle = cpu->sched_handle;
    new_task->sched_handle = entity;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched(cpu)->task_count++;
}

void add_eevdf_entity(tcb_t new_task, cpu_local_t *cpu) {
    struct sched_entity *entity = new_entity(new_task, NICE_TO_PRIO(0), cpu);
    new_task->sched_handle = entity;
    entity->handle = cpu->sched_handle;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched(cpu)->task_count++;
}

void add_eevdf_entity_prio(tcb_t new_task, cpu_local_t *cpu, uint64_t prio) {
    struct sched_entity *entity = new_entity(new_task, prio, cpu);
    new_task->sched_handle = entity;
    entity->handle = cpu->sched_handle;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched(cpu)->task_count++;
}

void remove_eevdf_entity(tcb_t thread, cpu_local_t *cpu) {
    if (thread == NULL || cpu == NULL || thread->sched_handle == NULL)
        return;
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    remove_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity, cpu);
    free(entity);
    thread->sched_handle = NULL;
    if (eevdf_sched(cpu)->task_count > 0)
        eevdf_sched(cpu)->task_count--;
}

void wait_eevdf_entity(tcb_t thread, cpu_local_t *cpu) {
    if (thread == NULL || cpu == NULL || thread->sched_handle == NULL)
        return;
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    if (!entity->on_rq)
        return;
    remove_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity, cpu);
    entity->wait_index = cow_list_add(((struct eevdf_t *)cpu->sched_handle)->wait_queue, entity);
    if (eevdf_sched(cpu)->task_count > 0)
        eevdf_sched(cpu)->task_count--;
}

void futex_eevdf_entity(tcb_t thread, cpu_local_t *cpu) {
    if (thread == NULL || cpu == NULL || thread->sched_handle == NULL)
        return;
    struct sched_entity *entity = (struct sched_entity *)thread->sched_handle;
    if (entity->on_rq)
        return;
    cow_list_remove(((struct eevdf_t *)cpu->sched_handle)->wait_queue, entity->wait_index);
    entity->handle = cpu->sched_handle;
    entity->on_rq = true;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, entity);
    eevdf_sched(cpu)->task_count++;
}

void init_cpu_idle(cpu_local_t *cpu, tcb_t ap_idle) {
    cpu->sched_handle = (struct eevdf_t *)calloc(1, sizeof(struct eevdf_t));
    ((struct eevdf_t *)cpu->sched_handle)->root =
        (struct rb_root *)calloc(1, sizeof(struct rb_root));
    ((struct eevdf_t *)cpu->sched_handle)->min_vruntime = 0;
    ((struct eevdf_t *)cpu->sched_handle)->wait_queue = cow_list_create();
    ((struct eevdf_t *)cpu->sched_handle)->task_count = 0;
    struct sched_entity *idle_entity = new_entity(ap_idle, NICE_TO_PRIO(20), cpu);
    ((struct eevdf_t *)cpu->sched_handle)->idle_entity = idle_entity;
    ap_idle->sched_handle = idle_entity;
    insert_sched_entity(((struct eevdf_t *)cpu->sched_handle)->root, idle_entity);
    eevdf_sched(cpu)->current = idle_entity;
}

// #pragma GCC pop_options
