#include "fpu.h"
#include "scheduler.h"
#include "io.h"
#include "krlibc.h"
#include "klog.h"

// 处理FPU异常的处理函数
void fpu_handler(registers_t *regs) {
    // 禁用FPU异常的响应，通过清除CR0寄存器的第2位和第3位
    set_cr0(get_cr0() & ~((1 << 2) | (1 << 3)));

    // 获取当前运行的进程
    pcb_t *current_proc = get_current_proc();

    // 检查该进程是否已经初始化过FPU
    if (!current_proc->fpu_flag) {
        // 如果尚未初始化，则清除FPU的错误状态并进行初始化
        __asm__ volatile("fnclex \n"  // 清除FPU错误状态
                     "fninit \n"      // 初始化FPU
                     :::"memory");

        // 将进程的FPU寄存器上下文重置为0
        memset(&(current_proc->context.fpu_regs), 0, sizeof(fpu_regs_t));
    } else {
        // 如果已经初始化，则从进程的上下文中恢复FPU状态
        __asm__ volatile("frstor (%%eax) \n" // 恢复FPU状态
                     ::"a"(&(current_proc->context.fpu_regs))  // 使用当前进程的FPU寄存器上下文
                     : "memory");
    }

    // 标记该进程已经初始化过FPU，避免重复处理
    current_proc->fpu_flag = 1;
}

// 切换FPU上下文时执行的函数
void switch_fpu(pcb_t *pcb) {
    // 禁用FPU异常响应，以便安全地保存或恢复状态
    set_cr0(get_cr0() & ~((1 << 2) | (1 << 3)));

    // 检查该进程是否使用了FPU
    if (pcb->fpu_flag) {
        // 如果使用了，则将当前的FPU状态保存到进程的上下文中
        __asm__ volatile("fnsave (%%eax) \n"  // 保存FPU状态
                     ::"a"(&(pcb->context.fpu_regs))  // 保存到指定内存位置
                     : "memory");
    }

    // 恢复CR0寄存器的第2位和第3位，重新启用FPU功能
    set_cr0(get_cr0() | (1 << 2) | (1 << 3));
}

// 初始化FPU环境
void init_fpu(void) {
    // 将FPU异常（中断号7）注册到fpu_handler处理函数
    register_interrupt_handler(7, fpu_handler);

    // 初始化FPU，确保其处于一个已知的初始状态
    __asm__ volatile("fninit");

    // 配置CR0寄存器，以指定FPU的操作模式：
    // 1 << 2：启用无助手模式（不使用FPU异常处理助手）
    // 1 << 3：设置响应优先级
    // 1 << 5：启用NEC SX87396单步模式（如有需要，根据具体硬件需求调整）
    set_cr0(get_cr0() | (1 << 2) | (1 << 3) | (1 << 5));

    // 输出日志信息，表示FPU协处理器已经初始化完成
    klogf(true, "FPU coprocessor is initialized.\n");
}
