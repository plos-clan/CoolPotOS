#pragma once

#define UART_CLOCK_FREQ 1843200

/* NS16550 寄存器偏移（逻辑偏移）*/
#define UART_RBR 0 // 接收缓冲寄存器
#define UART_THR 0 // 发送保持寄存器
#define UART_DLL 0 // 波特率除数低字节
#define UART_DLH 1 // 波特率除数高字节
#define UART_IER 1 // 中断使能寄存器
#define UART_IIR 2 // 中断识别寄存器
#define UART_FCR 2 // FIFO控制寄存器
#define UART_LCR 3 // 线路控制寄存器
#define UART_MCR 4 // 调制解调器控制寄存器
#define UART_LSR 5 // 线路状态寄存器
#define UART_MSR 6 // 调制解调器状态寄存器
#define UART_SCR 7 // 暂存寄存器

/* 寄存器位定义（与之前相同）*/
#define UART_LCR_DLAB       0x80
#define UART_LCR_WLEN8      0x03
#define UART_LSR_TEMT       0x40
#define UART_LSR_THRE       0x20
#define UART_LSR_DR         0x01
#define UART_FCR_ENABLE     0x01
#define UART_FCR_CLEAR_RX   0x02
#define UART_FCR_CLEAR_TX   0x04
#define UART_FCR_TRIGGER_14 0xC0
#define UART_MCR_DTR        0x01
#define UART_MCR_RTS        0x02
#define UART_MCR_OUT2       0x08

#include "types.h"

/* 访问宽度枚举 */
typedef enum {
    UART_ACCESS_8BIT  = 1, // 8位访问
    UART_ACCESS_16BIT = 2, // 16位访问
    UART_ACCESS_32BIT = 4  // 32位访问
} uart_access_width_t;

/* 地址空间类型 */
typedef enum {
    UART_ADDR_SPACE_MEMORY = 0, // 内存映射IO
    UART_ADDR_SPACE_IO     = 1  // 端口IO（x86）
} uart_addr_space_t;

/* UART配置结构体 */
typedef struct {
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    bool fifo_enable;
} uart_config_t;

/* UART设备结构体（支持GAS）*/
typedef struct {
    volatile void *base_addr;         // 基地址
    uart_addr_space_t addr_space;     // 地址空间类型
    uart_access_width_t access_width; // 访问宽度
    uint32_t reg_shift;               // 寄存器偏移位移（字节间距 = 1 << reg_shift）
    uint32_t reg_stride;              // 寄存器步进（优先使用）
    uint32_t clock_freq;              // 时钟频率
    uart_config_t config;             // 配置参数
} uart_device_t;

/* 函数声明 */
void uart_init(uart_device_t *uart, volatile void *base_addr, uart_config_t *config);
void uart_init_gas(
    uart_device_t *uart,
    volatile void *base_addr,
    uint32_t reg_shift,
    uart_access_width_t access_width,
    uart_config_t *config
);
void uart_set_baudrate(uart_device_t *uart, uint32_t baudrate);
void uart_putc(uart_device_t *uart, char c);
char uart_getc(uart_device_t *uart);
bool uart_data_available(uart_device_t *uart);
bool uart_is_transmit_empty(uart_device_t *uart);
void uart_puts(uart_device_t *uart, const char *str);
int uart_try_getc(uart_device_t *uart, char *c);
