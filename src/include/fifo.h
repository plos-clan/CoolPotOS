#ifndef CRASHPOWEROS_FIFO_H
#define CRASHPOWEROS_FIFO_H

#define FLAGS_OVERRUN 0x0001

struct FIFO8 {
    unsigned char *buf;
    int p, q, size, free, flags;
};

int fifo8_put(struct FIFO8* fifo, unsigned char data);
int fifo8_get(struct FIFO8* fifo);
void fifo8_init(struct FIFO8* fifo, int size, unsigned char* buf);

#endif
