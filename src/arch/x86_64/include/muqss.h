#pragma once

/* Priority ranges */
#define MAX_RT_PRIO   100
#define MAX_PRIO      140
#define MAX_USER_PRIO (MAX_PRIO - MAX_RT_PRIO)

/* Nice value ranges */
#define MIN_NICE   (-20)
#define MAX_NICE   19
#define NICE_WIDTH (MAX_NICE - MIN_NICE + 1) /* 40 */

/* Default values */
#define DEFAULT_PRIO 120
#define DEFAULT_NICE 0

/* Convert nice value to static priority */
#define NICE_TO_PRIO(nice) ((nice) + DEFAULT_PRIO)

/* Convert static priority to nice value */
#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIO)

/* Convert user priority (0-39) to static priority (100-139) */
#define USER_PRIO(prio) ((prio) + MAX_RT_PRIO)

/* Convert static priority to user priority */
#define TASK_USER_PRIO(prio) ((prio) - MAX_RT_PRIO)

/* Check if priority is in real-time range */
#define RT_PRIO(prio) ((prio) < MAX_RT_PRIO)

/* Check if priority is in normal range */
#define NORMAL_PRIO(prio) ((prio) >= MAX_RT_PRIO && (prio) < MAX_PRIO)

/* Validate nice value is in range */
#define NICE_IN_RANGE(nice) ((nice) >= MIN_NICE && (nice) <= MAX_NICE)

/* Validate priority is in range */
#define PRIO_IN_RANGE(prio) ((prio) >= 0 && (prio) < MAX_PRIO)

/* Clamp nice value to valid range */
#define CLAMP_NICE(nice) ((nice) < MIN_NICE ? MIN_NICE : ((nice) > MAX_NICE ? MAX_NICE : (nice)))

/* Clamp priority to valid range */
#define CLAMP_PRIO(prio) ((prio) < 0 ? 0 : ((prio) >= MAX_PRIO ? (MAX_PRIO - 1) : (prio)))

/* Nice 0 load weight */
#define NICE_0_LOAD 1024

/* Get weight for a nice value */
#define NICE_TO_WEIGHT(nice) (sched_prio_to_weight[(nice) - MIN_NICE])

/* Get weight multiplier for a nice value */
#define NICE_TO_WMULT(nice) (sched_prio_to_wmult[(nice) - MIN_NICE])

/* Get weight for a priority */
#define PRIO_TO_WEIGHT(prio) (sched_prio_to_weight[PRIO_TO_NICE(prio) - MIN_NICE])

/* Get weight multiplier for a priority */
#define PRIO_TO_WMULT(prio) (sched_prio_to_wmult[PRIO_TO_NICE(prio) - MIN_NICE])

/* ====================================================================
 * Time Slice Calculations
 * ====================================================================
 */

/* Minimum time slice (in milliseconds) */
#define MIN_TIMESLICE 5

/* Maximum time slice (in milliseconds) */
#define MAX_TIMESLICE 100

/* Default time slice for nice 0 */
#define DEF_TIMESLICE 20

/*
 * Calculate time slice based on priority
 * Higher priority (lower value) = longer time slice
 */
#define PRIO_TO_TIMESLICE(prio)                                                                    \
    (MIN_TIMESLICE + ((MAX_PRIO - 1 - (prio)) * (MAX_TIMESLICE - MIN_TIMESLICE) / (MAX_PRIO - 1)))

#define NICE_TO_TIMESLICE(nice) PRIO_TO_TIMESLICE(NICE_TO_PRIO(nice))

/* ====================================================================
 * Interactive/Latency Calculations
 * ====================================================================
 */

/* Calculate latency weight (for interactive tasks) */
#define PRIO_TO_LAT_WEIGHT(prio) (MAX_PRIO - (prio))

/* Check if task is interactive (nice < 0) */
#define IS_INTERACTIVE(nice) ((nice) < 0)

/* Check if task is batch/background (nice > 10) */
#define IS_BATCH(nice) ((nice) > 10)

/*
 * Scale delta time by task weight
 * Used to calculate virtual runtime in fair schedulers
 *
 * vruntime_delta = (delta_exec * NICE_0_LOAD) / task_weight
 */
#define CALC_DELTA(delta_exec, weight) (((delta_exec) * NICE_0_LOAD) / (weight))

#define NICE_DELTA(delta_exec, nice) CALC_DELTA(delta_exec, NICE_TO_WEIGHT(nice))

#define PRIO_DELTA(delta_exec, prio) CALC_DELTA(delta_exec, PRIO_TO_WEIGHT(prio))

#define MAX_LEVEL 8

/* Scheduling parameters */
#define DEFAULT_TIMESLICE 10 /* ms */
#define VRUNTIME_SCALE    1000000

#include "cptype.h"
#include "krlibc.h"
#include "pcb.h"

/* Forward declarations */
struct skiplist_node;
struct rq;

/* Scheduling entity - per-task scheduling information */
struct sched_entity {
    tcb_t    task;             /* Back pointer to task */
    uint64_t vruntime;         /* Virtual runtime (weighted) */
    uint64_t deadline;         /* Virtual deadline */
    uint64_t time_slice;       /* Remaining time slice (ms) */
    uint64_t exec_start;       /* Last execution start time */
    uint64_t sum_exec_runtime; /* Total execution time */

    int nice;
    int weight;

    bool is_yield;

    struct skiplist_node *node; /* Node in the skiplist */
    struct rq            *rq;   /* Run queue this entity is on */
};

/* Skip list node */
struct skiplist_node {
    struct sched_entity  *se;
    struct skiplist_node *forward[MAX_LEVEL];
    int                   level;
};

/* Run queue (per-CPU) */
struct rq {
    struct skiplist_node *head;
    int                   nr_running;   /* Number of tasks in queue */
    int                   max_level;    /* Current maximum level */
    uint64_t              clock;        /* Run queue clock */
    tcb_t                 curr;         /* Currently running task */
    tcb_t                 idle;         /* Idle task */
    uint64_t              min_vruntime; /* Minimum vruntime in queue */

    /* Statistics */
    uint64_t nr_switches;
    uint64_t total_runtime;
};

extern struct rq schedulers[MAX_CPU];
