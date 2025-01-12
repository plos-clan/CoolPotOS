#pragma once

#include "ctype.h"

#define close_interrupt __asm__ volatile("cli":::"memory")
#define open_interrupt __asm__ volatile("sti":::"memory")

static inline void io_out8(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void flush_tlb(uint64_t addr) {
    __asm__ volatile("invlpg (%0)"::"r" (addr) : "memory");
}

static inline void set_cr3(uint64_t cr0) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

static inline uint64_t get_cr3(void) {
    uint64_t cr0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr0));
    return cr0;
}
