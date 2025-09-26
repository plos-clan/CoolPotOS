#include "scheduler.h"
#include "cpustats.h"
#include "eevdf.h"
#include "fsgsbase.h"
#include "klog.h"
#include "krlibc.h"
#include "lock.h"
#include "lock_queue.h"
#include "pcb.h"
#include "signal.h"
#include "smp.h"
#include "timer.h"

tcb_t         kernel_head_task = NULL;
volatile bool is_scheduler     = false;

extern uint64_t cpu_count;        // smp.c
extern uint32_t bsp_processor_id; // smp.c

spin_t scheduler_lock;

tcb_t get_current_task() {
    return current_cpu->ready ? current_cpu->current_pcb : NULL;
}

void enable_scheduler() {
    is_scheduler = true;
}

void disable_scheduler() {
    is_scheduler = false;
}

void scheduler_yield() {
    if ((!current_cpu->ready) || current_cpu->current_pcb == NULL) return;
    open_interrupt;
    ((struct sched_entity *)get_current_task()->sched_handle)->is_yield = true;
    __asm__ volatile("int %0" ::"i"(timer));
}

void scheduler_nano_sleep(uint64_t nano) {
    uint64_t targetTime        = nano_time();
    uint64_t after             = 0;
    get_current_task()->status = WAIT;
    loop {
        uint64_t n = nano_time();
        if (n < targetTime) {
            after      += UINT64_MAX - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) {
            get_current_task()->status = RUNNING;
            return;
        }
        if (nano > 10) {
            scheduler_yield(); // 让出CPU时间片
        }
    }
}

/*
 * CP_Kernel 内核默认任务分配器 - 公平任务核心分配
 * 保证每一个CPU核心的任务数量大致相同
 */
int add_task(tcb_t new_task) {
    if (new_task == NULL) return -1;
    spin_lock(scheduler_lock);

    smp_cpu_t *cpu0  = get_cpu_smp(bsp_processor_id);
    uint32_t   cpuid = bsp_processor_id;

    for (size_t i = 0; i < cpu_count; i++) {
        smp_cpu_t *cpu = get_cpu_smp(i);
        if (cpu == NULL) continue;
        if (cpu->ready == 1 && cpu->scheduler_queue->size < cpu0->scheduler_queue->size) {
            cpu0  = cpu;
            cpuid = i;
        }
    }

    if (cpu0 == NULL) {
        spin_unlock(scheduler_lock);
        return -1;
    }
    close_interrupt;
    disable_scheduler();
    add_eevdf_entity(new_task, cpu0);
    open_interrupt;
    enable_scheduler();
    new_task->cpu_id      = cpuid;
    new_task->queue_index = lock_queue_enqueue(cpu0->scheduler_queue, new_task);
    spin_unlock(cpu0->scheduler_queue->lock);

    if (new_task->queue_index == (size_t)-1) { return -1; }

    spin_unlock(scheduler_lock);
    return new_task->queue_index;
}

int add_task_cpu(tcb_t new_task, size_t cpuid) {
    if (new_task == NULL) return -1;
    spin_lock(scheduler_lock);

    smp_cpu_t *cpu0 = get_cpu_smp(cpuid);

    if (cpu0 == NULL) {
        spin_unlock(scheduler_lock);
        return -1;
    }
    new_task->cpu_id = cpuid;
    add_eevdf_entity_with_prio(new_task, NICE_TO_PRIO(0), cpu0);

    new_task->queue_index = lock_queue_enqueue(cpu0->scheduler_queue, new_task);
    spin_unlock(cpu0->scheduler_queue->lock);

    if (new_task->queue_index == (size_t)-1) { return -1; }

    spin_unlock(scheduler_lock);
    return new_task->queue_index;
}

void remove_task(tcb_t task) {
    if (task == NULL) return;
    spin_lock(scheduler_lock);

    smp_cpu_t *smp = get_cpu_smp(task->cpu_id);
    not_null_assets(smp, "remove task null");
    queue_remove_at(smp->scheduler_queue, task->queue_index);
    spin_unlock(scheduler_lock);
}

int get_all_task() {
    return ((smp_cpu_t *)read_kgsbase())->ready
               ? ((smp_cpu_t *)read_kgsbase())->scheduler_queue->size
               : 0;
}

void change_current_tcb(tcb_t new_tcb) {
    ((smp_cpu_t *)read_kgsbase())->current_pcb = new_tcb;
    //write_fsbase((uint64_t)new_tcb);
}

void change_proccess(registers_t *reg, tcb_t current_task0, tcb_t target) {
    switch_page_directory(target->parent_group->page_dir);
    set_kernel_stack(target->kernel_stack);

    __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(target->fs));
    write_fsbase(target->fs_base);

    __asm__ __volatile__("movq %0, %%gs\n\t" ::"r"(target->gs));
    write_gsbase(target->gs_base);

    save_fpu_context(&current_task0->fpu_context);
    restore_fpu_context(&target->fpu_context);

    current_task0->context0.r15    = reg->r15;
    current_task0->context0.r14    = reg->r14;
    current_task0->context0.r13    = reg->r13;
    current_task0->context0.r12    = reg->r12;
    current_task0->context0.r11    = reg->r11;
    current_task0->context0.r10    = reg->r10;
    current_task0->context0.r9     = reg->r9;
    current_task0->context0.r8     = reg->r8;
    current_task0->context0.rax    = reg->rax;
    current_task0->context0.rbx    = reg->rbx;
    current_task0->context0.rcx    = reg->rcx;
    current_task0->context0.rdx    = reg->rdx;
    current_task0->context0.rdi    = reg->rdi;
    current_task0->context0.rsi    = reg->rsi;
    current_task0->context0.rbp    = reg->rbp;
    current_task0->context0.rflags = reg->rflags;
    current_task0->context0.rip    = reg->rip;
    current_task0->context0.rsp    = reg->rsp;
    current_task0->context0.ss     = reg->ss;
    current_task0->context0.es     = reg->es;
    current_task0->context0.cs     = reg->cs;
    current_task0->context0.ds     = reg->ds;

    reg->r15    = target->context0.r15;
    reg->r14    = target->context0.r14;
    reg->r13    = target->context0.r13;
    reg->r12    = target->context0.r12;
    reg->r11    = target->context0.r11;
    reg->r10    = target->context0.r10;
    reg->r9     = target->context0.r9;
    reg->r8     = target->context0.r8;
    reg->rax    = target->context0.rax;
    reg->rbx    = target->context0.rbx;
    reg->rcx    = target->context0.rcx;
    reg->rdx    = target->context0.rdx;
    reg->rdi    = target->context0.rdi;
    reg->rsi    = target->context0.rsi;
    reg->rbp    = target->context0.rbp;
    reg->rflags = target->context0.rflags;
    reg->rip    = target->context0.rip;
    reg->rsp    = target->context0.rsp;
    reg->ss     = target->context0.ss;
    reg->es     = target->context0.es;
    reg->ds     = target->context0.ds;
    reg->cs     = target->context0.cs;
}

// 仅作 gdb 调试信息输出用, 不参与内核逻辑实现
USED volatile int is_debug = 0;
void              for_each_rb_tree();

void scheduler(registers_t *reg) {
    if (!is_scheduler) return;
    if (!current_cpu->ready) {
        logkf("Error: scheduler null %d\n", current_cpu->id);
        return;
    }
    if (current_cpu->current_pcb == NULL) {
        logkf("Error: scheduler null %d\n", current_cpu->id);
        return;
    }

    tcb_t current = get_current_task();
    if (is_debug) for_each_rb_tree();
    tcb_t best = pick_next_task();
    if (best == current) return;
    write_fsbase((uint64_t)get_current_task()); // 下面要用内核态的fs，换上

    // 任务寻父处理
    extern pcb_t kernel_group;
    if (best->parent_group->parent_task == NULL ||
        best->parent_group->parent_task->status == DEATH ||
        best->parent_group->parent_task->status == OUT) {
        best->parent_group->parent_task = kernel_group;
    }

    // 信号检查
    check_pending_signals(best->parent_group, best);

    // 正式切换
    if (current_cpu->current_pcb != best) {
        if (current->status == RUNNING) { current->status = START; }
        if (best->status == START || best->status == CREATE) { best->status = RUNNING; }

        change_proccess(reg, current_cpu->current_pcb, best);
        change_current_tcb(best);
        current_cpu->current_pcb = best;
    } else
        write_fsbase(get_current_task()->fs_base); // 同样，没切任务，换回来
}
