#include "timer.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "scheduler.h"

// 全局时钟滴答计数器，volatile 关键字防止编译器优化
volatile uint32_t tick = 0;

/**
 * @brief 时钟中断处理函数
 *
 *  此函数在每次时钟中断发生时被调用。它会：
 *  1. 禁用中断 (io_cli)
 *  2. 增加全局时钟滴答计数器 (tick)
 *  3. 调用调度器 (scheduler_process) 进行进程调度
 *  4. 重新启用中断 (io_sti)
 *
 * @param regs 指向 CPU 寄存器状态的指针，用于保存和恢复进程上下文
 */
static void timer_handle(registers_t *regs) {
    io_cli();                // 禁用中断，防止中断嵌套导致问题
    tick++;                  // 增加全局时钟滴答计数
    scheduler_process(regs); // 调用调度器，进行进程切换
    io_sti();                // 重新启用中断
}

/**
 * @brief 时钟睡眠函数
 * 此函数提供了一个简单的忙等待延迟机制，使当前进程休眠指定的时钟滴答数。
 * @param timer 要休眠的时钟滴答数
 */
void clock_sleep(uint32_t timer) {
    io_sti();                      // 启用中断，确保 tick 计数器能够递增
    uint32_t sleep = tick + timer; // 计算休眠结束时的 tick 值
    while (tick < sleep)
        ; // 忙等待，直到 tick 达到目标值
}

/**
 * @brief 初始化定时器 (PIT - Programmable Interval Timer)
 *
 *  此函数配置 8253/8254 可编程间隔定时器 (PIT) 以产生周期性的时钟中断。
 *
 * @param timer  期望的时钟中断频率 (Hz)。  PIT 的基本频率是 1193180 Hz。
 *               实际的中断频率将是 1193180 / timer。
 */
void init_timer(uint32_t timer) {
    // 注册时钟中断处理函数 timer_handle 到 IRQ0
    register_interrupt_handler(IRQ0, &timer_handle);

    // 计算分频系数，PIT 的基本频率是 1193180 Hz
    uint32_t divisor = 1193180 / timer;

    // 设置 PIT 的工作模式
    // 0x43 是 PIT 的控制端口
    // 0x36 设置为：
    //   - 通道 0 (Channel 0)
    //   - 先发送低字节，再发送高字节 (Lobyte/Hibyte)
    //   - 模式 3 (方波发生器 - Square Wave Generator)
    //   - 16 位二进制计数器 (16-bit binary)
    outb(0x43, 0x36); // 写入控制字到端口 0x43

    // 将分频系数的低字节和高字节分别写入 PIT 的通道 0 数据端口 (0x40)
    uint8_t l = (uint8_t)(divisor & 0xFF);        // 获取分频系数的低字节
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF); // 获取分频系数的高字节

    outb(0x40, l); // 将低字节写入端口 0x40
    outb(0x40, h); // 将高字节写入端口 0x40
}
