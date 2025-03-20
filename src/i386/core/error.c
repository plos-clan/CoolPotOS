#include "error.h"
#include "isr.h"
#include "scheduler.h"
#include "klog.h"
#include "io.h"

// 处理除法错误异常（INT 0）
static void DE_0(registers_t *reg) {
    // 获取当前运行的进程控制块（PCB）
    pcb_t *pcb = get_current_proc();

    // 检查是否为内核任务或未找到有效的PCB
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        // 记录内核中的除法错误日志
        klogf(false, "Kernel has divide error.\n");

        // 禁用中断，避免进一步的中断干扰
        io_cli();

        // 进入死循环，停止处理器执行
        while (1)
            io_hlt();  // 执行HALT指令，暂停 CPU
    } else {
        // 记录用户进程中的除法错误日志
        klogf(false, "[%s:%d] has divide error.\n", pcb->name, pcb->pid);

        // 终止发生异常的进程
        kill_proc(pcb);
    }
}

// 处理栈段故障异常（INT 12）
static void SS_12(registers_t *reg) {
    // 获取当前运行的进程控制块（PCB）
    pcb_t *pcb = get_current_proc();

    // 检查是否为内核任务或未找到有效的PCB
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        // 记录内核中的栈段故障日志
        klogf(false, "Kernel has stack segment fault.\n");

        // 禁用中断，避免进一步的中断干扰
        io_cli();

        // 进入死循环，停止处理器执行
        while (1)
            io_hlt();  // 执行HALT指令，暂停 CPU
    } else {
        // 记录用户进程中的栈段故障日志
        klogf(false, "[%s:%d] has stack segment fault.\n", pcb->name, pcb->pid);

        // 终止发生异常的进程
        kill_proc(pcb);
    }
}

// 处理一般保护异常（INT 13）
static void GP_13(registers_t *reg) {
    // 获取当前运行的进程控制块（PCB）
    pcb_t *pcb = get_current_proc();

    // 检查是否为内核任务或未找到有效的PCB
    if (pcb == NULL || pcb->task_level == TASK_KERNEL_LEVEL) {
        // 记录内核中的一般保护异常日志
        klogf(false, "Kernel has General Protection.\n");

        // 禁用中断，避免进一步的中断干扰
        io_cli();

        // 进入死循环，停止处理器执行
        while (1)
            io_hlt();  // 执行HALT指令，暂停 CPU
    } else {
        // 记录用户进程中的一般保护异常日志
        klogf(false, "[%s:%d] has General Protection.\n", pcb->name, pcb->pid);

        // 终止发生异常的进程
        kill_proc(pcb);
    }
}

// 注册中断处理函数，用于处理特定的硬件异常
void setup_error() {
    // 将除法错误（INT 0）的处理函数注册为DE_0
    register_interrupt_handler(0, DE_0);

    // 将栈段故障（INT 12）的处理函数注册为SS_12
    register_interrupt_handler(12, SS_12);

    // 将一般保护异常（INT 13）的处理函数注册为GP_13
    register_interrupt_handler(13, GP_13);
}
