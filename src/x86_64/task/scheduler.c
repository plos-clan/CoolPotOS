#include "scheduler.h"
#include "fsgsbase.h"
#include "io.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "lock_queue.h"
#include "pcb.h"
#include "signal.h"
#include "smp.h"

tcb_t         kernel_head_task = NULL;
volatile bool is_scheduler     = false;

extern uint64_t cpu_count;        // smp.c
extern uint32_t bsp_processor_id; // smp.c

spin_t scheduler_lock;

tcb_t get_current_task() {
    return cpu->ready ? cpu->current_pcb : NULL;
}

void enable_scheduler() {
    is_scheduler = true;
}

void disable_scheduler() {
    is_scheduler = false;
}

void scheduler_yield() {
    if ((!cpu->ready) || cpu->current_pcb == NULL) return;
    __asm__ volatile("int %0" ::"i"(timer));
}

void scheduler_nano_sleep(uint64_t nano) {
    uint64_t targetTime = nano_time();
    uint64_t after      = 0;
    loop {
        uint64_t n = nano_time();
        if (n < targetTime) {
            after      += 0xffffffff - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) { return; }
        if (nano < 10) {
            scheduler_yield(); // 让出CPU时间片
        }
    }
}

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
    new_task->cpu_id      = cpuid;
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

/**
 * CP_Kernel 内核默认多核调度器 - 循环绝对公平调度
 * @param reg 当前任务上下文
 */
void scheduler(registers_t *reg) {
    if (!is_scheduler) return;
    if (!cpu->ready) {
        logkf("Error: scheduler null %d\n", cpu->id);
        return;
    }
    if (cpu->current_pcb == NULL) {
        logkf("Error: scheduler null %d\n", cpu->id);
        return;
    }

    write_fsbase((uint64_t)get_current_task()); // 下面要用内核态的fs，换上

    tcb->cpu_clock++;
    if (tcb->time_buf != NULL) {
        tcb->cpu_timer += get_time(tcb->time_buf);
        tcb->time_buf   = NULL;
    }
    tcb->time_buf = alloc_timer();

    // 下一任务选取
    tcb_t next;
    if (cpu->scheduler_queue->size == 1) {
        write_fsbase(get_current_task()->fs_base); // 没有进行任务切换，再换回来
        spin_unlock(scheduler_lock);
        return;
    }
    if (cpu->iter_node == NULL) {
    iter_head:
        cpu->iter_node = cpu->scheduler_queue->head;
        next           = (tcb_t)cpu->iter_node->data;
    } else {
    resche:
        cpu->iter_node = cpu->iter_node->next;
        if (cpu->iter_node == NULL) goto iter_head;
        next = (tcb_t)cpu->iter_node->data;
        not_null_assets(next, "scheduler next null");

        // 死亡/回收/挂起的线程不调度
        switch (next->status) {
        case FUTEX:
        case DEATH:
        case OUT: goto resche;
        default:
            if (next->parent_group->status == DEATH || next->parent_group->status == FUTEX) {
                goto resche;
            }
        }
    }

    // 任务寻父处理
    extern pcb_t kernel_group;
    if (next->parent_group->parent_task == NULL ||
        next->parent_group->parent_task->status == DEATH ||
        next->parent_group->parent_task->status == OUT) {
        next->parent_group->parent_task = kernel_group;
    }

    // 信号检查
    check_pending_signals(next->parent_group, next);

    // 正式切换
    if (cpu->current_pcb != next) {
        disable_scheduler();
        if (tcb->status == RUNNING) tcb->status = START;
        if (next->status == START || next->status == CREATE) {
            next->status               = RUNNING;
            next->parent_group->status = RUNNING;
        }

        change_proccess(reg, cpu->current_pcb, next);
        change_current_tcb(next);
        enable_scheduler();
    } else
        write_fsbase(get_current_task()->fs_base); // 同样，没切任务，换回来
}
