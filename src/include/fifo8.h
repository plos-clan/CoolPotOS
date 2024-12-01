#pragma once

#define FLAGS_OVERRUN 0x0001

struct FIFO8 {
    unsigned int *buf;
    int p, q, size, free, flags;
};

int fifo8_put(struct FIFO8 *fifo, unsigned int data); // 向指定缓冲队列加入数据
int fifo8_get(struct FIFO8 *fifo); // 获取指定缓冲队列末尾
void fifo8_init(struct FIFO8 *fifo, int size, unsigned int *buf); // 初始化指定缓冲队列