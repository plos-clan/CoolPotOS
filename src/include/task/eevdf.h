#pragma once

#define MAX(x, y) ((x > y) ? (x) : (y))

#define MAX_NICE   19
#define MIN_NICE   -20
#define NICE_WIDTH (MAX_NICE - MIN_NICE + 1)

#define MAX_RT_PRIO 100
#define MAX_DL_PRIO 0

#define MAX_PRIO     (MAX_RT_PRIO + NICE_WIDTH)
#define DEFAULT_PRIO (MAX_RT_PRIO + NICE_WIDTH / 2)

#define NICE_TO_PRIO(nice) ((nice) + DEFAULT_PRIO)
#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIO)

#define SCHED_FIXEDPOINT_SHIFT 10
#define SCHED_FIXEDPOINT_SCALE (1L << SCHED_FIXEDPOINT_SHIFT)

#define SCHED_CAPACITY_SHIFT SCHED_FIXEDPOINT_SHIFT
#define SCHED_CAPACITY_SCALE (1L << SCHED_CAPACITY_SHIFT)

#define NICE_0_LOAD_SHIFT (SCHED_FIXEDPOINT_SHIFT + SCHED_FIXEDPOINT_SHIFT)
#define scale_load(w)     ((w) << SCHED_FIXEDPOINT_SHIFT)
#define scale_load_down(w)                                                                         \
    ({                                                                                             \
        unsigned long __w = (w);                                                                   \
                                                                                                   \
        if (__w) __w = MAX(2UL, __w >> SCHED_FIXEDPOINT_SHIFT);                                    \
        __w;                                                                                       \
    })

#define WEIGHT_IDLEPRIO 3
#define WMULT_IDLEPRIO  1431655765

#define NICE_0_LOAD (1L << NICE_0_LOAD_SHIFT)

#define WMULT_CONST (~0U)
#define WMULT_SHIFT 32

#define eevdf_sched(cpu) ((struct eevdf_t *)cpu->sched_handle)

#include "cow_arraylist.h"
#include "rbtree.h"
#include "smp.h"
#include "task.h"
#include "types.h"

extern unsigned int    sysctl_sched_base_slice;
typedef struct eevdf_t eevdf_t;

struct load_weight {
    uint64_t weight;
    uint32_t inv_weight;
};

struct sched_entity {
    uint64_t           prio;
    uint64_t           vruntime;
    uint64_t           slice;
    uint64_t           custom_slice;
    uint64_t           deadline;
    uint64_t           exec_start;
    uint64_t           min_vruntime;
    uint64_t           sum_exec_runtime;
    bool               is_idle; // 是否是IDLE进程
    struct load_weight load;
    struct rb_node     run_node;
    bool               on_rq;      // 是否就绪
    tcb_t              thread;     // 任务句柄
    size_t             wait_index; // 等待队列索引
    bool               is_yield;   // 是否是yield任务
    eevdf_t           *handle;     // EEVDF调度环境
};

struct eevdf_t {
    struct rb_root      *root;
    struct sched_entity *current;     // 当前调度单元
    struct sched_entity *idle_entity; // IDLE调度单元
    cow_arraylist       *wait_queue;  // 阻塞队列
    size_t               task_count;  // 当前调度单元数量

    uint64_t avg_load;
    uint64_t avg_vruntime;
    uint64_t min_vruntime;
};

void  add_eevdf_entity_with_prio(tcb_t new_task, uint64_t prio, cpu_local_t *cpu);
void init_cpu_idle(cpu_local_t *cpu, tcb_t ap_idle);
tcb_t eevdf_pick_next_task(cpu_local_t *cpu);
