#include "fifo8.h"

void fifo8_init(struct FIFO8* fifo, int size, unsigned int* buf) {
    fifo->size = size;
    fifo->buf = buf;
    fifo->free = size; /* 缓冲区大小 */
    fifo->flags = 0;
    fifo->p = 0; /* 下一个数据写入位置 */
    fifo->q = 0; /* 下一个数据读出位置 */
    return;
}

int fifo8_put(struct FIFO8* fifo, unsigned int data) {
    if (fifo->free == 0) {
        /* 没有空间了，溢出 */
        fifo->flags |= FLAGS_OVERRUN;
        return -1;
    }
    fifo->buf[fifo->p] = data;
    fifo->p++;
    if (fifo->p == fifo->size) {
        fifo->p = 0;
    }
    fifo->free--;
    return 0;
}

int fifo8_get(struct FIFO8* fifo) {
    int data;
    if (fifo->free == fifo->size) {
        /* 如果缓冲区为空则返回-1 */
        return -1;
    }
    data = fifo->buf[fifo->q];
    fifo->q++;
    if (fifo->q == fifo->size) {
        fifo->q = 0;
    }
    fifo->free++;
    return data;
}