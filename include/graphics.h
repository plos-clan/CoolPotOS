#ifndef CPOS_VGA_H
#define CPOS_VGA_H

#include "printf.h"
#include "multiboot.h"

/*
 * \032 DARK GRAY
 * \033 LIGHT RED
 * \034 LIGHT BLUE
 * \035 LIGHT GREEN
 * \036 LIGHT GRAY
 * \037 LIGHT CYAN
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define FLAG_ALTNT_FORM 0x01
#define FLAG_ALTNT_FORM_CH '#'

#define FLAG_ZERO_PAD 0x02
#define FLAG_ZERO_PAD_CH '0'

#define FLAG_LEFT_ADJUST 0x04
#define FLAG_LEFT_ADJUST_CH '-'

#define FLAG_SPACE_BEFORE_POS_NUM 0x08
#define FLAG_SPACE_BEFORE_POS_NUM_CH ' '

#define FLAG_SIGN 0x10
#define FLAG_SIGN_CH '+'

#define FLAG_LOWER 0x20

#define INT_TYPE_CHAR 0x1
#define INT_TYPE_SHORT 0x2
#define INT_TYPE_INT 0x4
#define INT_TYPE_LONG 0x8
#define INT_TYPE_LONG_LONG 0x10
#define INT_TYPE_MIN INT_TYPE_CHAR
#define INT_TYPE_MAX INT_TYPE_LONG_LONG

#define BUF_SIZE 4096

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

struct color_rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

typedef struct color_rgba color_rgba;

void vga_install(void);

void vga_setcolor(uint8_t color);

void vga_putentryat(char c, uint8_t color, size_t x, size_t y);

void vga_putchar(char c);

void vga_write(const char *data, size_t size);

void vga_writestring(const char *data);

void vga_write_dec(uint32_t dec);

uint16_t vga_entry(unsigned char uc, uint8_t color);

void vga_clear();

void move_cursor();

void printf(const char *formet, ...);

void vbe_putchar(char c);

void vbe_clear();

void initVBE(multiboot_t *mboot);

void putPix(unsigned int x, unsigned int y, uint32_t color);

#endif