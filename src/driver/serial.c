#include "serial.h"

int init_serial() {
    io_out8(SERIAL_PORT + 1, 0x00); // 禁止COM的中断发生
    io_out8(SERIAL_PORT + 3, 0x80); // 启用DLAB（设置波特率除数）。
    io_out8(SERIAL_PORT + 0, 0x03); // 设置除数为3，(低位) 38400波特
    io_out8(SERIAL_PORT + 1, 0x00); //            (高位)
    io_out8(SERIAL_PORT + 3, 0x03); // 8位，无奇偶性，一个停止位
    io_out8(SERIAL_PORT + 2, 0xC7); // 启用FIFO，有14字节的阈值
    io_out8(SERIAL_PORT + 4, 0x0B); // 启用IRQ，设置RTS/DSR
    io_out8(SERIAL_PORT + 4, 0x1E); // 设置为环回模式，测试串口
    io_out8(SERIAL_PORT + 0, 0xAE); // 测试串口（发送字节0xAE并检查串口是否返回相同的字节）

    // 检查串口是否有问题（即：与发送的字节不一样）
    if (io_in8(SERIAL_PORT + 0) != 0xAE) {
        return 1;
    }

    // 如果串口没有故障，将其设置为正常运行模式。
    // (非环回，启用IRQ，启用OUT#1和OUT#2位)
    io_out8(SERIAL_PORT + 4, 0x0F);
    return 0;
}

char read_serial() {
    while ((io_in8(SERIAL_PORT + 5) & 1) == 0);
    return io_in8(SERIAL_PORT);
}

void write_serial(char a) {
    while ((io_in8(SERIAL_PORT + 5) & 0x20) == 0);
    io_out8(SERIAL_PORT, a);
}
