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

#define eevdf_sched ((struct eevdf_t *)current_cpu->sched_handle)

#include "ctype.h"
#include "fsgsbase.h"
#include "lock_queue.h"
#include "rbtree.h"
#include "smp.h"

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
    lock_queue          *wait_queue;  // 阻塞队列
    size_t               task_count;  // 当前调度单元数量

    uint64_t avg_load;
    uint64_t avg_vruntime;
    uint64_t min_vruntime;
};

/**
 * 以指定优先级生成一个调度单元
 * @param task 任务句柄
 * @param prio 优先级
 * @return NULL ? 生成失败 : 调度单元
 */
struct sched_entity *new_entity(tcb_t task, uint64_t prio, smp_cpu_t *cpu);

/**
 * 将一个调度单元添加到树(会触发红黑树重排)
 * @param root 树根节点
 * @param se 调度单元
 */
void insert_sched_entity(struct rb_root *root, struct sched_entity *se);

/**
 * 更新当前调度单元的信息 (deadline vruntime)
 */
void update_current_task();

/**
 * 从红黑树中删除一个调度单元
 * @param root 树根节点
 * @param se 调度单元
 */
void remove_sched_entity(struct rb_root *root, struct sched_entity *se);

/**
 * 选取下一个可调度的单元
 * @return 调度单元
 */
struct sched_entity *pick_eevdf();

/**
 * 初始化多核下的EEVDF调度环境
 * @param cpu 添加的CPU核心
 * @param ap_idle 该核心的IDLE任务
 */
void init_ap_idle(smp_cpu_t *cpu, tcb_t ap_idle);

/**
 * 选取下一个可调度的任务
 * @return 任务句柄
 */
tcb_t pick_next_task();

/**
 * 添加一个任务进红黑树
 * @param new_task 新任务
 * @param cpu 任务被添加的CPU核心
 */
void add_eevdf_entity(tcb_t new_task, smp_cpu_t *cpu);

/**
 * 添加一个任务进红黑树(指定优先级)
 * @param new_task 新任务
 * @param prio 优先级
 * @param cpu 任务被添加的CPU核心
 */
void add_eevdf_entity_with_prio(tcb_t new_task, uint64_t prio, smp_cpu_t *cpu);

/**
 * 删除一个任务
 * @param thread 任务
 * @param cpu 被删除的CPU核心
 */
void remove_eevdf_entity(tcb_t thread, smp_cpu_t *cpu);

/**
 * 将任务移入等待队列(不可被调度)
 * @param thread 任务本体
 * @param cpu 任务所在CPU
 */
void wait_eevdf_entity(tcb_t thread, smp_cpu_t *cpu);

/**
 * 将任务移入红黑树(调度就绪)
 * @param thread 任务本体
 * @param cpu 任务所在CPU
 */
void futex_eevdf_entity(tcb_t thread, smp_cpu_t *cpu);

/**
 * 调整CPU0的IDLE进程为正常权重
 */
void change_bsp_weight();

/**
 * 调整指定线程的权重
 * @param thread 线程实体
 * @param prio 任务优先级
 */
void change_entity_weight(tcb_t thread, uint64_t prio);
