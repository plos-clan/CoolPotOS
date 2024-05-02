#ifndef CRASHPOWEROS_IO_H
#define CRASHPOWEROS_IO_H


#include <stdint.h>

struct tty {
    int using1;                              // 使用标志
    void *vram;                              // 显存（也可以当做图层）
    int x, y;                                // 目前的 x y 坐标
    int xsize, ysize;                        // x 坐标大小 y 坐标大小
    int Raw_y;                               // 换行次数
    int cur_moving;                          // 光标需要移动吗
    unsigned char color;                     // 颜色
    void (*putchar)(struct tty *res, int c); // putchar函数
    void (*MoveCursor)(struct tty *res, int x, int y);  // MoveCursor函数
    void (*clear)(struct tty *res);                     // clear函数
    void (*screen_ne)(struct tty *res);                 // screen_ne函数
    void (*gotoxy)(struct tty *res, int x, int y);      // gotoxy函数
    void (*print)(struct tty *res, const char *string); // print函数
    void (*Draw_Box)(struct tty *res, int x, int y, int x1, int y1,
                     unsigned char color); // Draw_Box函数
    int (*fifo_status)(struct tty *res);
    int (*fifo_get)(struct tty *res);
    unsigned int reserved[4]; // 保留项
};

void gdt_flush(uint32_t gdtr);

void io_hlt(void);

void io_cli(void);

void io_sti(void);

void io_stihlt(void);

int io_in8(int port);

int io_in16(int port);

int io_in32(int port);

void io_out8(int port, int data);

void io_out16(int port, int data);

void io_out32(int port, int data);

int io_load_eflags(void);

void io_store_eflags(int eflags);

void load_gdtr(int limit, int addr);

void load_idtr(int limit, int addr);

uint16_t inw(uint16_t port);

void copy_page_physical(uint32_t,uint32_t);

extern char read_port(unsigned short port);

static inline void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt) {
    asm volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void insw(uint16_t port, void *addr, uint32_t word_cnt) {
    asm volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
}

#endif //CRASHPOWEROS_IO_H
