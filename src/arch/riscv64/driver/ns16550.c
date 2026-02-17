#include "driver/ns16550.h"

/* 计算实际寄存器偏移 */
static inline uint32_t uart_calc_offset(uart_device_t *uart, uint8_t reg) {
    if (uart->reg_stride > 0) {
        return reg * uart->reg_stride;
    }
    return reg << uart->reg_shift;
}

/* 读寄存器 - 支持不同访问宽度 */
static uint32_t uart_read_reg(uart_device_t *uart, uint8_t reg) {
    uint32_t offset     = uart_calc_offset(uart, reg);
    volatile void *addr = (volatile char *)uart->base_addr + offset;

    switch (uart->access_width) {
    case UART_ACCESS_8BIT:
        return *(volatile uint8_t *)addr;
    case UART_ACCESS_16BIT:
        return *(volatile uint16_t *)addr;
    case UART_ACCESS_32BIT:
        return *(volatile uint32_t *)addr;
    default:
        return *(volatile uint8_t *)addr;
    }
}

/* 写寄存器 - 支持不同访问宽度 */
static void uart_write_reg(uart_device_t *uart, uint8_t reg, uint32_t value) {
    uint32_t offset     = uart_calc_offset(uart, reg);
    volatile void *addr = (volatile char *)uart->base_addr + offset;

    switch (uart->access_width) {
    case UART_ACCESS_8BIT:
        *(volatile uint8_t *)addr = (uint8_t)value;
        break;
    case UART_ACCESS_16BIT:
        *(volatile uint16_t *)addr = (uint16_t)value;
        break;
    case UART_ACCESS_32BIT:
        *(volatile uint32_t *)addr = value;
        break;
    default:
        *(volatile uint8_t *)addr = (uint8_t)value;
        break;
    }
}

/**
 * @brief 标准NS16550初始化（寄存器连续）
 */
void uart_init(uart_device_t *uart, volatile void *base_addr, uart_config_t *config) {
    uart_init_gas(uart, base_addr, 0, UART_ACCESS_8BIT, config);
}

/**
 * @brief NS16550_GAS初始化（支持寄存器间距）
 * @param uart UART设备结构体指针
 * @param base_addr 寄存器基地址
 * @param reg_shift 寄存器偏移位移（0=连续, 2=每4字节）
 * @param access_width 访问宽度
 * @param config 配置参数
 */
void uart_init_gas(
    uart_device_t *uart,
    volatile void *base_addr,
    uint32_t reg_shift,
    uart_access_width_t access_width,
    uart_config_t *config
) {
    if (uart == NULL || base_addr == NULL) {
        return;
    }

    uart->base_addr    = base_addr;
    uart->addr_space   = UART_ADDR_SPACE_MEMORY;
    uart->access_width = access_width;
    uart->reg_shift    = reg_shift;
    uart->reg_stride   = 0; // 优先使用reg_shift
    uart->clock_freq   = UART_CLOCK_FREQ;

    /* 默认配置 */
    if (config == NULL) {
        uart->config.baudrate    = 115200;
        uart->config.data_bits   = UART_LCR_WLEN8;
        uart->config.stop_bits   = 0;
        uart->config.parity      = 0;
        uart->config.fifo_enable = true;
    } else {
        uart->config = *config;
    }

    /* 禁用所有中断 */
    uart_write_reg(uart, UART_IER, 0x00);

    /* 设置波特率 */
    uart_set_baudrate(uart, uart->config.baudrate);

    /* 配置线路控制寄存器 */
    uint8_t lcr = uart->config.data_bits | uart->config.stop_bits | uart->config.parity;
    uart_write_reg(uart, UART_LCR, lcr);

    /* 配置FIFO */
    if (uart->config.fifo_enable) {
        uart_write_reg(
            uart,
            UART_FCR,
            UART_FCR_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX | UART_FCR_TRIGGER_14
        );
    }

    /* 配置调制解调器控制寄存器 */
    uart_write_reg(uart, UART_MCR, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);
}

/**
 * @brief 设置波特率
 */
void uart_set_baudrate(uart_device_t *uart, uint32_t baudrate) {
    if (uart == NULL || baudrate == 0) {
        return;
    }

    uint16_t divisor = (uint16_t)(uart->clock_freq / (16 * baudrate));

    uint8_t lcr = uart_read_reg(uart, UART_LCR);
    uart_write_reg(uart, UART_LCR, lcr | UART_LCR_DLAB);
    uart_write_reg(uart, UART_DLL, divisor & 0xFF);
    uart_write_reg(uart, UART_DLH, (divisor >> 8) & 0xFF);
    uart_write_reg(uart, UART_LCR, lcr);

    uart->config.baudrate = baudrate;
}

/**
 * @brief 检查发送缓冲区是否为空
 */
bool uart_is_transmit_empty(uart_device_t *uart) {
    if (uart == NULL)
        return false;
    return (uart_read_reg(uart, UART_LSR) & UART_LSR_THRE) != 0;
}

/**
 * @brief 发送一个字符
 */
void uart_putc(uart_device_t *uart, char c) {
    if (uart == NULL)
        return;
    while (!uart_is_transmit_empty(uart))
        ;
    uart_write_reg(uart, UART_THR, (uint8_t)c);
}

/**
 * @brief 检查是否有数据可读
 */
bool uart_data_available(uart_device_t *uart) {
    if (uart == NULL)
        return false;
    return (uart_read_reg(uart, UART_LSR) & UART_LSR_DR) != 0;
}

/**
 * @brief 接收一个字符（阻塞）
 */
char uart_getc(uart_device_t *uart) {
    if (uart == NULL)
        return 0;
    while (!uart_data_available(uart))
        ;
    return (char)uart_read_reg(uart, UART_RBR);
}

/**
 * @brief 尝试接收一个字符（非阻塞）
 */
int uart_try_getc(uart_device_t *uart, char *c) {
    if (uart == NULL || c == NULL)
        return -1;
    if (uart_data_available(uart)) {
        *c = (char)uart_read_reg(uart, UART_RBR);
        return 0;
    }
    return -1;
}

/**
 * @brief 发送字符串
 */
void uart_puts(uart_device_t *uart, const char *str) {
    if (uart == NULL || str == NULL)
        return;
    while (*str) {
        if (*str == '\n')
            uart_putc(uart, '\r');
        uart_putc(uart, *str++);
    }
}
