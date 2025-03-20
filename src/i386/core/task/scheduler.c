#include "scheduler.h"
#include "description_table.h"
#include "fpu.h"
#include "io.h"
#include "klog.h"
#include "krlibc.h"
#include "page.h"

extern void switch_to(struct context *prev, struct context *next); //asmfunc.asm

pcb_t *current_pcb       = NULL; //当前运行进程
pcb_t *running_proc_head = NULL; //调度队列
pcb_t *wait_proc_head    = NULL; //等待队列

bool can_sche = false; //调度标志位

int get_all_task() {
    int    num = 1;
    pcb_t *pcb = running_proc_head;
    do {
        pcb = pcb->next;
        if (pcb == NULL) break;
        num++;
    } while (1); //遍历进程控制器列表获取进程总数
    return num;
}

void enable_scheduler() { //启动调度器
    can_sche = true;
}

void disable_scheduler() { //关闭调度器
    can_sche = false;
}

pcb_t *get_current_proc() { //获取PCB指针
    return current_pcb;
}

void kernel_sche() {
    __asm__("int $31\n");
}

/**
 * @brief 调度器处理函数，用于管理当前进程的调度和状态更新。
 *
 * @param reg 系统寄存器指针，包含与进程切换相关的寄存器内容。
 */
void scheduler_process(registers_t *reg) {
    // 禁用全局中断，以进入临界区确保操作的原子性
    io_cli();

    // 检查当前是否存在可运行的进程并且调度功能被启用
    if (current_pcb && can_sche) {
        // 记录调度器状态，主要用于调试和日志输出（注释中的logkf调用已被移除）

        // 检查当前进程是否处于死亡状态
        if (current_pcb->status == DEATH) {
            // 当前进程死亡，将其设置为下一个可运行的进程
            current_pcb = running_proc_head;

            // 记录日志，确认下一个进程的状态
            logkf("DEATH SCHEDULER %s\n", current_pcb->next->status == DEATH ? "YE" : "NO");
        }

        // 增加当前进程的CPU时钟计数器，用于跟踪使用的时间片
        current_pcb->cpu_clock++;

        // 调用默认调度函数，以执行实际的进程切换或其他调度逻辑
        default_scheduler(reg, current_pcb->next);

        /*
         * 以下为待实现的功能，根据任务层级选择不同的调度策略
         */
        /*
        if(current_pcb->task_level == TASK_KERNEL_LEVEL){
            // 对于内核级任务，执行特定的调度逻辑
            default_scheduler(reg,current_pcb->next);
        } else{
            // 对于用户级线程，尚未实现具体的调度策略
            // TODO：开发用户线程的调度功能
            default_scheduler(reg,current_pcb->next);
        }
        */
    }
}

/**
 * @brief 默认调度器函数，负责执行进程切换的具体操作。
 *
 * @param reg   包含当前寄存器状态的指针，用于保存和恢复上下文。
 * @param next  指向下一个要运行的进程控制块（PCB）的指针。
 */
void default_scheduler(registers_t *reg, pcb_t *next) {
    // 检查当前进程是否还有剩余的时间片
    if (current_pcb->sche_time > 1) {
        // 如果有，减少时间片计数器，并返回，不执行进程切换
        current_pcb->sche_time--;
        return;
    }

    // 确保当前进程与下一个进程不同，避免不必要的切换
    if (current_pcb != next) {
        // 如果下一个进程为NULL，设置为就绪队列的头部，确保循环调度
        if (next == NULL) next = running_proc_head;

        // 将当前进程的时间片重置为1次，可能在切换回该进程时使用
        current_pcb->sche_time = 1;

        // 保存当前进程的PCB，以便后续恢复其上下文
        pcb_t *prev = current_pcb;

        // 更新当前进程指针为下一个要执行的进程
        current_pcb = next;

        // 切换到新进程的页目录，更新地址空间
        switch_page_directory(current_pcb->pgd_dir);

        // 设置内核栈指针到新进程的内核栈顶部
        set_kernel_stack((uintptr_t)current_pcb->kernel_stack + STACK_SIZE);

        // 切换FPU（浮点单元）状态到新进程的上下文
        switch_fpu(current_pcb);

        // 保存当前寄存器状态到前一个进程的上下文中
        prev->context.eip = reg->eip; // 指令指针
        prev->context.ds  = reg->ds;  // 数据段寄存器
        prev->context.cs  = reg->cs;  // 代码段寄存器
        prev->context.eax = reg->eax; // 通用寄存器EAX的值
        prev->context.ss  = reg->ss;  // 堆栈段寄存器

        // 执行上下文切换，将CPU状态从前一个进程切换到新进程
        switch_to(&(prev->context), &(current_pcb->context));

        // 更新寄存器为新进程的上下文，准备执行新进程
        reg->ds  = current_pcb->context.ds;
        reg->cs  = current_pcb->context.cs;
        reg->eip = current_pcb->context.eip;
        reg->eax = current_pcb->context.eax;
        reg->ss  = current_pcb->context.ss;
    }
}
