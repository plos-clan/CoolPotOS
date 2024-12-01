#pragma once

#include "ctypes.h"

typedef struct {
    unsigned short di, si, bp, sp, bx, dx, cx, ax;
    unsigned short gs, fs, es, ds, eflags;
} regs16_t;

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

extern char read_port(unsigned short port);

static inline uint32_t get_cr0(void) {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

static inline void set_cr0(uint32_t cr0) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt) {
    __asm__ volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void insw(uint16_t port, void *addr, uint32_t word_cnt) {
    __asm__ volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
}
