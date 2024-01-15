#ifndef CPOS_IO_H
#define CPOS_IO_H

#include <stdint.h>

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

extern char read_port(unsigned short port);

static inline void outb(uint16_t port,uint8_t data){
    asm volatile ("outb %b0, %w1" : : "a"(data) , "Nd" (port));
}

static inline void outsw(uint16_t port,const void *addr,uint32_t word_cnt){
    asm volatile ("cld; rep outsw" : "+S"(addr),"+c"(word_cnt):"d"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t data;
    asm volatile ("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void insw(uint16_t port,void *addr,uint32_t word_cnt){
    asm volatile ("cld; rep insw":"+D"(addr),"+c"(word_cnt):"d"(port):"memory");
}

#endif