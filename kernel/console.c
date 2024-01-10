#include "../include/console.h"
#include "../include/common.h"
#include "../include/kheap.h"
#include "../include/io.h"
#include <stdarg.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

static uint16_t cursor_x = 0, cursor_y = 0; // 光标位置

static inline uint8_t

vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t

vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)
    uc | (uint16_t)
    color << 8;
}

void move_cursor() {
    uint16_t cursorLocation = cursor_y * 80 + cursor_x; // 当前光标位置
    outb(0x3D4, 14); // 光标高8位
    outb(0x3D5, cursorLocation >> 8); // 写入
    outb(0x3D4, 15); // 光标低8位
    outb(0x3D5, cursorLocation); // 写入，由于value声明的是uint8_t，因此会自动截断
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*)0xB8000;
    //terminal_buffer = (uint16_t) VARTUAL_GRAPHICS_ADDRESS;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

static void scroll() {
    uint8_t attributeByte = (0 << 4) | (15 & 0x0F);
    uint16_t blank = 0x20 | (attributeByte << 8);

    if (cursor_y >= 25) {
        int i;
        for (i = 0 * 80; i < 24 * 80; i++) terminal_buffer[i] = terminal_buffer[i + 80];
        for (i = 24 * 80; i < 25 * 80; i++) terminal_buffer[i] = blank;
        cursor_y = 24;
    }
}

void terminal_clear() {
    uint8_t attributeByte = (0 << 4) | (15 & 0x0F); // 黑底白字
    uint16_t blank = 0x20 | (attributeByte << 8); // 0x20 -> 空格这个字，attributeByte << 8 -> 属性位

    for (int i = 0; i < 80 * 25; i++) terminal_buffer[i] = blank; // 全部打印为空格

    cursor_x = 0;
    cursor_y = 0;
    move_cursor(); // 光标置于左上角
}

static void redpane_putchar(char c) {
    uint8_t backColor = 0b0000100, foreColor = 15;
    uint8_t attributeByte = (backColor << 4) | (foreColor & 0x0f); // 黑底白字
    uint16_t attribute = attributeByte << 8;
    uint16_t * location;

    if (c == 0x08 && cursor_x) {
        cursor_x--;
    } else if (c == 0x09) {
        location = terminal_buffer + (cursor_y * 80 + cursor_x);
        *location = ' ' | attribute;
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\n') {
        cursor_x = 0; // 光标回首
        cursor_y++; // 下一行
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
    move_cursor();
}

void redpane_println(char *s) {
    redpane_print(s);
    redpane_putchar('\n');
}

void redpane_print(char *s) {
    for (; *s; s++) redpane_putchar(*s);
}

void redpane_print_hex(uint32_t hex) {
    char buf[20]; // 32位最多0xffffffff，20个都多了
    char *p = buf; // 用于写入的指针
    char ch; // 当前十六进制字符
    int i, flag = 0; // i -> 循环变量，flag -> 前导0是否结束

    *p++ = '0';
    *p++ = 'x'; // 先存一个0x

    if (hex == 0) *p++ = '0'; // 如果是0，直接0x0结束
    else {
        for (i = 28; i >= 0; i -= 4) { // 每次4位
            ch = (hex >> i) & 0xF; // 0~9, A~F
            // 28的原因是多留一点后路（
            if (flag || ch > 0) { // 跳过前导0
                flag = 1; // 没有前导0就把flag设为1，这样后面再有0也不会忽略
                ch += '0'; // 0~9 => '0'~'9'
                if (ch > '9') {
                    ch += 7; // 'A' - '9' = 7
                }
                *p++ = ch; // 写入
            }
        }
    }
    *p = '\0'; // 结束符

    redpane_print(buf);
}

void terminal_putchar(char c) {
    uint8_t backColor = 0, foreColor = 15;
    uint8_t attributeByte = (backColor << 4) | (foreColor & 0x0f); // 黑底白字
    uint16_t attribute = attributeByte << 8;
    uint16_t * location;

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
        cursor_y++; // 下一行
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
    move_cursor();
}

void terminal_write_dec(uint32_t dec) {
    int upper = dec / 10, rest = dec % 10;
    if (upper) terminal_write_dec(upper);
    terminal_putchar(rest + '0');
}

void terminal_write_hex(uint32_t hex) {
    char buf[20]; // 32位最多0xffffffff，20个都多了
    char *p = buf; // 用于写入的指针
    char ch; // 当前十六进制字符
    int i, flag = 0; // i -> 循环变量，flag -> 前导0是否结束

    *p++ = '0';
    *p++ = 'x'; // 先存一个0x

    if (hex == 0) *p++ = '0'; // 如果是0，直接0x0结束
    else {
        for (i = 28; i >= 0; i -= 4) { // 每次4位
            ch = (hex >> i) & 0xF; // 0~9, A~F
            // 28的原因是多留一点后路（
            if (flag || ch > 0) { // 跳过前导0
                flag = 1; // 没有前导0就把flag设为1，这样后面再有0也不会忽略
                ch += '0'; // 0~9 => '0'~'9'
                if (ch > '9') {
                    ch += 7; // 'A' - '9' = 7
                }
                *p++ = ch; // 写入
            }
        }
    }
    *p = '\0'; // 结束符

    terminal_writestring(buf);
}

void terminal_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char *data) {
    terminal_write(data, strlen(data));
}

void println(const char *data) {
    terminal_writestring(data);
    terminal_putchar('\n');
}

void printf(const char *formet, ...) {
    int len;
    va_list ap;
    va_start(ap, formet);
    char *buf = (char *) alloc(1024);
    len = vsprintf(buf, formet, ap);
    terminal_writestring(buf);
    va_end(ap);
    kfree(buf);
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *str = buf;
    int flag = 0;
    int int_type = INT_TYPE_INT;
    int tot_width = 0;
    int sub_width = 0;
    char buf2[64] = {0};
    char *s = NULL;
    char ch = 0;
    int8_t num_8 = 0;
    uint8_t num_u8 = 0;
    int16_t num_16 = 0;
    uint16_t num_u16 = 0;
    int32_t num_32 = 0;
    uint32_t num_u32 = 0;
    int64_t num_64 = 0;
    uint64_t num_u64 = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            *str++ = *p;
            continue;
        }

        flag = 0;
        tot_width = 0;
        sub_width = 0;
        int_type = INT_TYPE_INT;

        p++;

        while (*p == FLAG_ALTNT_FORM_CH || *p == FLAG_ZERO_PAD_CH ||
               *p == FLAG_LEFT_ADJUST_CH || *p == FLAG_SPACE_BEFORE_POS_NUM_CH ||
               *p == FLAG_SIGN_CH) {
            if (*p == FLAG_ALTNT_FORM_CH) {
                flag |= FLAG_ALTNT_FORM;
            } else if (*p == FLAG_ZERO_PAD_CH) {
                flag |= FLAG_ZERO_PAD;
            } else if (*p == FLAG_LEFT_ADJUST_CH) {
                flag |= FLAG_LEFT_ADJUST;
                flag &= ~FLAG_ZERO_PAD;
            } else if (*p == FLAG_SPACE_BEFORE_POS_NUM_CH) {
                flag |= FLAG_SPACE_BEFORE_POS_NUM;
            } else if (*p == FLAG_SIGN_CH) {
                flag |= FLAG_SIGN;
            } else {
            }

            p++;
        }

        if (*p == '*') {
            tot_width = va_arg(args,
            int);
            if (tot_width < 0)
                tot_width = 0;
            p++;
        } else {
            while (isdigit(*p)) {
                tot_width = tot_width * 10 + *p - '0';
                p++;
            }
        }
        if (*p == '.') {
            if (*p == '*') {
                sub_width = va_arg(args,
                int);
                if (sub_width < 0)
                    sub_width = 0;
                p++;
            } else {
                while (isdigit(*p)) {
                    sub_width = sub_width * 10 + *p - '0';
                    p++;
                }
            }
        }

        LOOP_switch:
        switch (*p) {
            case 'h':
                p++;
                if (int_type >= INT_TYPE_MIN) {
                    int_type >>= 1;
                    goto LOOP_switch;
                } else {
                    *str++ = '%';
                    break;
                }
            case 'l':
                p++;
                if (int_type <= INT_TYPE_MAX) {
                    int_type <<= 1;
                    goto LOOP_switch;
                } else {
                    *str++ = '%';
                    break;
                }
            case 's':
                s = va_arg(args,
                char*);
                str = insert_str(str, s);
                break;
            case 'c':
                ch = (char) (va_arg(args,
                int) &0xFF);
                *str++ = ch;
                break;
            case 'd':
                switch (int_type) {
                    case INT_TYPE_CHAR:
                        num_8 = (int8_t) va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_8, flag, tot_width));
                        break;
                    case INT_TYPE_SHORT:
                        num_16 = (int16_t) va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_16, flag, tot_width));
                        break;
                    case INT_TYPE_INT:
                        num_32 = va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_32, flag, tot_width));
                        break;
                    case INT_TYPE_LONG:
                        num_64 = va_arg(args, int64_t);
                        str = insert_str(str, int64_to_str_dec(num_64, flag, tot_width));
                        break;
                    case INT_TYPE_LONG_LONG:
                        num_64 = va_arg(args, int64_t);
                        str = insert_str(str, int64_to_str_dec(num_64, flag, tot_width));
                        break;
                }
                break;
            case 'x':
                flag |= FLAG_LOWER;
            case 'X':
                switch (int_type) {
                    case INT_TYPE_CHAR:
                        num_u8 = (uint8_t)
                        va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u8, flag, tot_width));
                        break;
                    case INT_TYPE_SHORT:
                        num_u16 = (uint16_t)
                        va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u16, flag, tot_width));
                        break;
                    case INT_TYPE_INT:
                        num_u32 = va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u32, flag, tot_width));
                        break;
                    case INT_TYPE_LONG:
                        num_u64 = va_arg(args, uint64_t);
                        str = insert_str(str, uint64_to_str_hex(num_u64, flag, tot_width));
                        break;
                    case INT_TYPE_LONG_LONG:
                        num_u64 = va_arg(args, uint64_t);
                        str = insert_str(str, uint64_to_str_hex(num_u64, flag, tot_width));
                        break;
                }
                break;
            case 'o':
                num_u32 = va_arg(args, uint32_t);
                str = insert_str(str, uint32_to_str_oct(num_u32, flag, tot_width));
                break;
            case '%':
                *str++ = '%';
                break;
            default:
                *str++ = '%';
                *str++ = *p;
                break;
        }
    }
    *str = '\0';

    return str - buf;
}