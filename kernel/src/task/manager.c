#include "task/task.h"
#include "krlibc.h"
#include "mem/slub.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "cpu_local.h"
#include "term/kprint.h"

/* ---- 全局状态 ---- */
static struct llist_header g_task_list;       /* 所有任务 */
static task_queue_t        g_ready_queue;      /* 就绪队列 */
static task_t             *g_current_task;     /* 当前运行任务 */
static task_t             *g_idle_task;        /* 空闲任务 */
static uint64_t            g_next_pid = 1;     /* 下一个 PID */
static uint64_t            g_tick_count = 0;   /* 系统滴答计数 */
static spin_t              g_sched_lock = SPIN_INIT;

/* ---- 空闲任务 ---- */
static void idle_thread(void) {
    while (1) {
        arch_wait_for_interrupt();
        task_yield();
    }
}

/* ---- 调度器核心 ---- */

void schedule(void) {
    spin_lock(g_sched_lock);

    task_t *prev = g_current_task;
    task_t *next = NULL;

    /* 将当前任务放回就绪队列 (如果仍在运行状态) */
    if (prev && prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
        prev->time_slice = TASK_TIME_SLICE;
        llist_append(&g_ready_queue.head, &prev->run_node);
        g_ready_queue.count++;
    }

    /* 从就绪队列中取出下一个任务 */
    while (!llist_empty(&g_ready_queue.head)) {
        task_t *candidate = list_entry(g_ready_queue.head.next, task_t, run_node);
        llist_delete(&candidate->run_node);
        g_ready_queue.count--;

        if (candidate->state == TASK_READY) {
            next = candidate;
            break;
        }
        /* 跳过非就绪状态的任务 */
    }

    /* 如果没有就绪任务，使用空闲任务 */
    if (!next) {
        next = g_idle_task;
    }

    next->state = TASK_RUNNING;
    next->last_scheduled = g_tick_count;
    g_current_task = next;

    /* 更新 CPU 本地信息 */
    cpu_local_t *cpu = get_current_cpu();
    if (cpu) {
        cpu->current_task = next;
        cpu->current_dir  = next->page_dir ? next->page_dir : get_kernel_page_dir();
    }

    spin_unlock(g_sched_lock);
}

task_t *task_get_current(void) {
    return g_current_task;
}

void task_set_current(task_t *task) {
    g_current_task = task;
}

int task_get_pid(void) {
    return g_current_task ? (int)g_current_task->pid : -1;
}

/* ---- 任务管理 ---- */

void task_init(void) {
    llist_init_head(&g_task_list);
    llist_init_head(&g_ready_queue.head);
    g_ready_queue.lock  = SPIN_INIT;
    g_ready_queue.count = 0;
    g_next_pid = 1;
    g_tick_count = 0;

    /* 创建空闲任务 (最低优先级) */
    g_idle_task = task_create("idle", idle_thread, 1, TASK_TYPE_KERNEL);
    if (g_idle_task) {
        g_idle_task->state = TASK_READY;
        llist_append(&g_ready_queue.head, &g_idle_task->run_node);
        g_ready_queue.count++;
    }

    /* 设置当前为 idle */
    g_current_task = g_idle_task;
    kinfo("Task: scheduler initialized (round-robin)");
}

task_t *task_create(const char *name, void (*entry)(void), int priority, enum task_type type) {
    if (!name || !entry) return NULL;

    task_t *task = calloc(1, sizeof(task_t));
    if (!task) return NULL;

    /* 分配 PID (带重复检测) */
    spin_lock(g_sched_lock);
    uint64_t start_pid = g_next_pid;
    do {
        task->pid = g_next_pid++;
        if (g_next_pid > TASK_MAX_PID) g_next_pid = 1;
        /* 检查 PID 是否已存在 */
        if (!task_find_by_pid(task->pid)) break;
    } while (g_next_pid != start_pid);
    spin_unlock(g_sched_lock);

    if (g_next_pid == start_pid) {
        /* 所有 PID 都已用完 */
        free(task);
        return NULL;
    }

    /* 基本信息 */
    strncpy(task->name, name, TASK_NAME_LEN - 1);
    task->state     = TASK_READY;
    task->type      = type;
    task->priority  = (priority > 0 && priority <= TASK_MAX_PRIORITY) ? priority : TASK_DEFAULT_PRIORITY;
    task->time_slice = TASK_TIME_SLICE;
    task->exit_code = 0;

    /* 分配内核栈 */
    task->kernel_stack = alloc_frames(TASK_STACK_PAGES);
    if (task->kernel_stack == 0) {
        free(task);
        return NULL;
    }

    /* 设置内核栈 (栈顶 = 栈底 + 栈大小 - 上下文大小) */
    uint64_t *stack_top = (uint64_t *)(phys_to_virt(task->kernel_stack + TASK_STACK_PAGES * PAGE_SIZE));
    task_context_t *ctx = (task_context_t *)((uint8_t *)stack_top - sizeof(task_context_t));
    memset(ctx, 0, sizeof(task_context_t));

    /* 初始化上下文 */
    ctx->rip    = (uint64_t)entry;
    ctx->cs     = 0x08;  /* 内核代码段 */
    ctx->rflags = 0x202; /* IF 标志置位 */
    ctx->rsp    = (uint64_t)stack_top;
    ctx->ss     = 0x10;  /* 内核数据段 */
    ctx->rbp    = ctx->rsp;

    task->context = ctx;
    task->page_dir = get_kernel_page_dir();

    /* 加入全局链表 */
    llist_append(&g_task_list, &task->node);

    /* 加入就绪队列 */
    spin_lock(g_ready_queue.lock);
    llist_append(&g_ready_queue.head, &task->run_node);
    g_ready_queue.count++;
    spin_unlock(g_ready_queue.lock);

    return task;
}

void task_exit(int exit_code) {
    task_t *task = g_current_task;
    if (!task || task == g_idle_task) return;

    task->state     = TASK_ZOMBIE;
    task->exit_code = exit_code;

    kinfo("Task: '%s' (pid=%llu) exited with code %d", task->name, task->pid, exit_code);

    /* 切换调度 */
    task_yield();
    /* 不应该到达这里 */
    while (1) arch_wait_for_interrupt();
}

void task_yield(void) {
    schedule();
}

void task_sleep(uint64_t ms) {
    task_t *task = g_current_task;
    if (!task) return;

    task->state       = TASK_SLEEPING;
    task->sleep_until = g_tick_count + ms;
    task_yield();
}

void task_block(task_t *task) {
    if (!task) return;
    task->state = TASK_BLOCKED;
}

void task_unblock(task_t *task) {
    if (!task) return;
    if (task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
        spin_lock(g_ready_queue.lock);
        /* 防止重复添加：检查是否已在就绪队列中 */
        bool in_queue = false;
        task_t *rq, *rq_tmp;
        llist_for_each(rq, rq_tmp, &g_ready_queue.head, run_node) {
            if (rq == task) { in_queue = true; break; }
        }
        if (!in_queue) {
            llist_append(&g_ready_queue.head, &task->run_node);
            g_ready_queue.count++;
        }
        spin_unlock(g_ready_queue.lock);
    }
}

void task_wakeup(task_t *task) {
    if (!task) return;
    if (task->state == TASK_SLEEPING) {
        task->state = TASK_READY;
        spin_lock(g_ready_queue.lock);
        /* 防止重复添加 */
        bool in_queue = false;
        task_t *rq, *rq_tmp;
        llist_for_each(rq, rq_tmp, &g_ready_queue.head, run_node) {
            if (rq == task) { in_queue = true; break; }
        }
        if (!in_queue) {
            llist_append(&g_ready_queue.head, &task->run_node);
            g_ready_queue.count++;
        }
        spin_unlock(g_ready_queue.lock);
    }
}

/* ---- 查询 ---- */

task_t *task_find_by_pid(uint64_t pid) {
    task_t *task, *tmp;
    llist_for_each(task, tmp, &g_task_list, node) {
        if (task->pid == pid) return task;
    }
    return NULL;
}

int task_get_count(void) {
    spin_lock(g_sched_lock);
    int count = 0;
    task_t *task, *tmp;
    llist_for_each(task, tmp, &g_task_list, node) count++;
    spin_unlock(g_sched_lock);
    return count;
}

void task_list_all(void) {
    spin_lock(g_sched_lock);
    task_t *task, *tmp;
    printk("=== Task List ===\n");
    printk("PID\tNAME\t\tSTATE\tPRI\tTICKS\n");
    llist_for_each(task, tmp, &g_task_list, node) {
        const char *state_str = "?";
        switch (task->state) {
        case TASK_RUNNING:  state_str = "RUN"; break;
        case TASK_READY:    state_str = "RDY"; break;
        case TASK_BLOCKED:  state_str = "BLK"; break;
        case TASK_SLEEPING: state_str = "SLP"; break;
        case TASK_ZOMBIE:   state_str = "ZMB"; break;
        case TASK_DEAD:     state_str = "DED"; break;
        }
        printk("%llu\t%s\t\t%s\t%d\t%llu\n",
               task->pid, task->name, state_str, task->priority, task->total_ticks);
    }
    spin_unlock(g_sched_lock);
}

/* ---- 每 tick 调用 ---- */
void scheduler_tick(void) {
    g_tick_count++;

    /* 唤醒睡眠任务 (持锁遍历) */
    spin_lock(g_sched_lock);
    task_t *task, *tmp;
    llist_for_each(task, tmp, &g_task_list, node) {
        if (task->state == TASK_SLEEPING && g_tick_count >= task->sleep_until) {
            task->state = TASK_READY;
            /* 检查是否已在就绪队列中 */
            bool in_queue = false;
            task_t *rq, *rq_tmp;
            llist_for_each(rq, rq_tmp, &g_ready_queue.head, run_node) {
                if (rq == task) { in_queue = true; break; }
            }
            if (!in_queue) {
                llist_append(&g_ready_queue.head, &task->run_node);
                g_ready_queue.count++;
            }
        }
    }
    spin_unlock(g_sched_lock);

    /* 时间片递减 */
    if (g_current_task && g_current_task != g_idle_task) {
        g_current_task->total_ticks++;
        if (g_current_task->time_slice > 0) {
            g_current_task->time_slice--;
        }
        if (g_current_task->time_slice == 0) {
            task_yield();
        }
    }
}

void task_idle(void) {
    idle_thread();
}