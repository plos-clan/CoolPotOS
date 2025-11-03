#pragma once

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

#define __clamp(val, lo, hi) ((val) >= (hi) ? (hi) : ((val) <= (lo) ? (lo) : (val)))

#define __clamp_once(type, val, lo, hi, uval, ulo, uhi)                                            \
    ({                                                                                             \
        type uval = (val);                                                                         \
        type ulo  = (lo);                                                                          \
        type uhi  = (hi);                                                                          \
        __clamp(uval, ulo, uhi);                                                                   \
    })

#define __careful_clamp(type, val, lo, hi)                                                         \
    __clamp_once(type, val, lo, hi, __UNIQUE_ID(v_), __UNIQUE_ID(l_), __UNIQUE_ID(h_))

#define clamp(val, lo, hi) __careful_clamp(__auto_type, val, lo, hi)

#define vruntime_gt(field, lse, rse) ({ (int64_t)((lse)->field - (rse)->field) > 0; })

#define __node_2_se(node) \
	rb_entry((node), struct sched_entity, run_node)

#define VRUNTIME_OFFSET_THRESHOLD 0xa000000000000000

#include "cow_arraylist.h"
#include "rbtree.h"
#include "smp.h"
#include "task.h"
#include "types.h"
#include "scheduler.h"

extern unsigned int    sysctl_sched_base_slice;
typedef struct eevdf_t eevdf_t;

struct load_weight {
    uint64_t weight;
    uint32_t inv_weight;
};

struct sched_entity {
    uint64_t           prio;     // 调度优先级
    uint64_t           vruntime; // 虚拟时间
    uint64_t           slice;    // 时间片
    uint64_t           custom_slice;
    uint64_t           deadline;     // 虚拟截止时间
    uint64_t           exec_start;   // 此次调度开始执行的时间
    uint64_t           min_vruntime; // 子树下的 min_vruntime
    uint64_t           sum_exec_runtime;
    int64_t            vlag;    // min_vruntime 与 vruntime 的差值
    bool               is_idle; // 是否是IDLE进程
    struct load_weight load;
    struct rb_node     run_node;   // 红黑树节点
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

void  set_entity_yield(tcb_t thread);
void  change_entity_weight(tcb_t thread, uint64_t prio, cpu_local_t *cpu);
void  remove_eevdf_entity(tcb_t thread, cpu_local_t *cpu);
void  wait_eevdf_entity(tcb_t thread, cpu_local_t *cpu);
void  futex_eevdf_entity(tcb_t thread, cpu_local_t *cpu);
void  add_eevdf_entity_with_prio(tcb_t new_task, uint64_t prio, cpu_local_t *cpu);
void  init_cpu_idle(cpu_local_t *cpu, tcb_t ap_idle);
tcb_t eevdf_pick_next_task(cpu_local_t *cpu);
