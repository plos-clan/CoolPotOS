#include "scheduler.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "lock_queue.h"
#include "pcb.h"
#include "smp.h"

tcb_t kernel_head_task = NULL;
bool is_scheduler = false;

extern uint64_t cpu_count;        // smp.c
extern uint32_t bsp_processor_id; // smp.c

ticketlock scheduler_lock;

tcb_t get_current_task() {
    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    if (cpu == NULL) return NULL;
    return cpu->current_pcb;
}

void enable_scheduler() {
    is_scheduler = true;
}

void disable_scheduler() {
    is_scheduler = false;
}

int add_task(tcb_t new_task) {
    if (new_task == NULL) return -1;
    ticket_lock(&scheduler_lock);

    smp_cpu_t *cpu0 = get_cpu_smp(bsp_processor_id);
    uint32_t cpuid  = bsp_processor_id;

    for (size_t i = 0; i < cpu_count; i++) {
        smp_cpu_t *cpu = get_cpu_smp(i);
        if (cpu == NULL) continue;
        if (cpu->flags == 1 && cpu->scheduler_queue->size < cpu0->scheduler_queue->size) {
            cpu0  = cpu;
            cpuid = i;
        }
    }

    if (cpu0 == NULL) {
        ticket_unlock(&scheduler_lock);
        return -1;
    }
    new_task->cpu_id = cpuid;
    new_task->queue_index = lock_queue_enqueue(cpu0->scheduler_queue, new_task);
    ticket_unlock(&cpu0->scheduler_queue->lock);

    if (new_task->queue_index == (size_t) -1) {
        return -1;
    }

    ticket_unlock(&scheduler_lock);
    return new_task->queue_index;
}

void remove_task(tcb_t task) {
    if (task == NULL) return;
    ticket_lock(&scheduler_lock);

    smp_cpu_t *cpu = get_cpu_smp(task->cpu_id);
    if (cpu == NULL) {
        ticket_unlock(&scheduler_lock);
        return;
    }
    queue_remove_at(cpu->scheduler_queue, task->queue_index);
    ticket_unlock(&scheduler_lock);
}

int get_all_task() {
    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    return cpu != NULL ? cpu->scheduler_queue->size : 0;
}

void change_proccess(registers_t *reg, tcb_t current_task0, tcb_t taget) {
    switch_page_directory(taget->parent_group->page_dir);
    set_kernel_stack(taget->kernel_stack);

    save_fpu_context(&current_task0->fpu_context);
    restore_fpu_context(&taget->fpu_context);

    current_task0->context0.r15 = reg->r15;
    current_task0->context0.r14 = reg->r14;
    current_task0->context0.r13 = reg->r13;
    current_task0->context0.r12 = reg->r12;
    current_task0->context0.r11 = reg->r11;
    current_task0->context0.r10 = reg->r10;
    current_task0->context0.r9  = reg->r9;
    current_task0->context0.r8  = reg->r8;
    current_task0->context0.rax = reg->rax;
    current_task0->context0.rbx = reg->rbx;
    current_task0->context0.rcx = reg->rcx;
    current_task0->context0.rdx = reg->rdx;
    current_task0->context0.rdi = reg->rdi;
    current_task0->context0.rsi = reg->rsi;
    current_task0->context0.rbp = reg->rbp;
    current_task0->context0.rflags = reg->rflags;
    current_task0->context0.rip = reg->rip;
    current_task0->context0.rsp = reg->rsp;
    current_task0->context0.ss  = reg->ss;
    current_task0->context0.es  = reg->es;
    current_task0->context0.gs  = reg->gs;
    current_task0->context0.cs  = reg->cs;
    current_task0->context0.ds  = reg->ds;

    reg->r15 = taget->context0.r15;
    reg->r14 = taget->context0.r14;
    reg->r13 = taget->context0.r13;
    reg->r12 = taget->context0.r12;
    reg->r11 = taget->context0.r11;
    reg->r10 = taget->context0.r10;
    reg->r9  = taget->context0.r9;
    reg->r8  = taget->context0.r8;
    reg->rax = taget->context0.rax;
    reg->rbx = taget->context0.rbx;
    reg->rcx = taget->context0.rcx;
    reg->rdx = taget->context0.rdx;
    reg->rdi = taget->context0.rdi;
    reg->rsi = taget->context0.rsi;
    reg->rbp = taget->context0.rbp;
    reg->rflags = taget->context0.rflags;
    reg->rip = taget->context0.rip;
    reg->rsp = taget->context0.rsp;
    reg->ss  = taget->context0.ss;
    reg->es  = taget->context0.es;
    reg->ds  = taget->context0.ds;
    reg->cs  = taget->context0.cs;
    reg->gs  = taget->context0.gs;
}

/**
 * Cpinl 内核默认多核调度器 - 循环绝对公平调度
 * @param reg 当前任务上下文
 */
void scheduler(registers_t *reg) {
    if (is_scheduler) {
        ticket_lock(&scheduler_lock);
        smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
        if (cpu == NULL) {
            logkf("Error: scheduler null %d\n", get_current_cpuid());
            ticket_unlock(&scheduler_lock);
            return;
        }
        if (cpu->current_pcb != NULL) {
            cpu->current_pcb->cpu_clock++;
            if (cpu->current_pcb->time_buf != NULL) {
                cpu->current_pcb->cpu_timer += get_time(cpu->current_pcb->time_buf);
                cpu->current_pcb->time_buf   = NULL;
            }
            cpu->current_pcb->time_buf = alloc_timer();

            // 下一任务选取
            tcb_t next;
            if (cpu->scheduler_queue->size == 1) {
                ticket_unlock(&scheduler_lock);
                return;
            }
            if (cpu->iter_node == NULL) {
                iter_head:
                cpu->iter_node = cpu->scheduler_queue->head;
                next = (tcb_t)cpu->iter_node->data;
            } else {
                resche:
                cpu->iter_node = cpu->iter_node->next;
                if (cpu->iter_node == NULL) goto iter_head;
                next = (tcb_t) cpu->iter_node->data;
                not_null_assets(next);
                if(next->status == DEATH ||
                   next->status == OUT ||
                   next->parent_group->status == DEATH) goto resche;
            }

            //logkf("Scheduler: %d:%s -> %d:%s\n", cpu->current_pcb->status,cpu->current_pcb->name,next->status,next->name);

            // 正式切换
            if (cpu->current_pcb->parent_group != next->parent_group ||
                cpu->current_pcb->pid != next->pid) {
                disable_scheduler();
                if(cpu->current_pcb->status == RUNNING)
                    cpu->current_pcb->status = START;
                if(next->status == START || next->status == CREATE){
                    next->status = RUNNING;
                    next->parent_group->status = RUNNING;
                }

                change_proccess(reg, cpu->current_pcb, next);
                cpu->current_pcb = next;
                enable_scheduler();
            }
        }
        ticket_unlock(&scheduler_lock);
    }
}
