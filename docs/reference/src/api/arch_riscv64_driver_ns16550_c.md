# arch/riscv64/driver/ns16550.c

## `void uart_init(uart_device_t *uart, volatile void *base_addr, uart_config_t *config) {`


标准NS16550初始化（寄存器连续）


---

## `void uart_init_gas(uart_device_t *uart, volatile void *base_addr, uint32_t reg_shift, uart_access_width_t access_width, uart_config_t *config) {`


NS16550_GAS初始化（支持寄存器间距）

- **`uart`**: UART设备结构体指针
- **`base_addr`**: 寄存器基地址
- **`reg_shift`**: 寄存器偏移位移（0=连续, 2=每4字节）
- **`access_width`**: 访问宽度
- **`config`**: 配置参数


---

## `void uart_set_baudrate(uart_device_t *uart, uint32_t baudrate) {`


设置波特率


---

## `bool uart_is_transmit_empty(uart_device_t *uart) {`


检查发送缓冲区是否为空


---

## `void uart_putc(uart_device_t *uart, char c) {`


发送一个字符


---

## `bool uart_data_available(uart_device_t *uart) {`


检查是否有数据可读


---

## `char uart_getc(uart_device_t *uart) {`


接收一个字符（阻塞）


---

## `int uart_try_getc(uart_device_t *uart, char *c) {`


尝试接收一个字符（非阻塞）


---

## `void uart_puts(uart_device_t *uart, const char *str) {`


发送字符串


---

