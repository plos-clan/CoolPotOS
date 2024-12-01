#include "io.h"
#include "krlibc.h"
#include "video.h"


#define VGA_WIDTH 80
#define VGA_HEIGHT 25

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

int status = 0;

uint16_t cursor_x = 0, cursor_y = 0; // 光标位置

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

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

static void scroll() {
    uint8_t attributeByte = (0 << 4) | (15 & 0x0F);
    uint16_t blank = 0x20 | (attributeByte << 8);

    if (cursor_y >= 25) {
        int i;
        for (i = 0 * 80; i < 24 * 80; i++)
            terminal_buffer[i] = terminal_buffer[i + 80];
        for (i = 24 * 80; i < 25 * 80; i++)
            terminal_buffer[i] = blank;
        cursor_y = 24;
    }
}

void vga_install(void) {
    status = 0;
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t *)0xB8000;
    // terminal_buffer = (uint16_t*) 0xA0000;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void vga_clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_move_cursor(cursor_x, cursor_y);
}

void vga_move_cursor(uint16_t x, uint16_t y) {
    uint16_t cursorLocation = y * 80 + x;
    outb(0x3D4, 14);
    outb(0x3D5, cursorLocation >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, cursorLocation);
}

void vga_setcolor(uint8_t color) { terminal_color = color; }

void vga_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void vga_putchar(char c) {
    uint8_t attributeByte = terminal_color; // 黑底白字
    uint16_t attribute = attributeByte << 8;
    uint16_t *location;

    if (c == 0x08 && cursor_x) {
        cursor_x--;
        location = terminal_buffer + (cursor_y * 80 + cursor_x);
        *location = ' ' | attribute;
    } else if (c == 0x09) {
        location = terminal_buffer + (cursor_y * 80 + cursor_x);
        *location = ' ' | attribute;
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\n') {
        cursor_x = 0; // 光标回首
        cursor_y++;   // 下一行
    } else if (c >= ' ' && c <= '~') {
        location = terminal_buffer + (cursor_y * 80 + cursor_x);
        *location = c | attribute;
        cursor_x++;
    }

    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }

    scroll();
    vga_move_cursor(cursor_x, cursor_y);
}

void vga_write_dec(uint32_t dec) {
    int upper = dec / 10, rest = dec % 10;
    if (upper)
        vga_write_dec(upper);
    vga_putchar(rest + '0');
}

void vga_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        vga_putchar(data[i]);
}

void vga_writestring(const char *data) { vga_write(data, strlen(data)); }