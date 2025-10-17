#pragma once

#define KMSG_SIZE (1 << 17) // 内核日志环形缓冲区大小 (128k)

#define BLACK   0 // 黑
#define RED     1 // 红
#define GREEN   2 // 绿
#define YELLOW  3 // 黄
#define BLUE    4 // 蓝
#define MAGENTA 5 // 品红
#define CYAN    6 // 青
#define WHITE   7 // 白

#define ksuccess(...)                                                                              \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(GREEN, BLACK, " SUCCESS");                                                    \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kinfo(...)                                                                                 \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(CYAN, BLACK, "  INFO  ");                                                     \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kdebug(...)                                                                                \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(BLUE, BLACK, "DEBUG (%s:%d)", __FILE__, __LINE__);                            \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kwarn(...)                                                                                 \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(YELLOW, BLACK, "  WARN  ");                                                   \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kerror(...)                                                                                \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(RED, BLACK, " FAILED ");                                                      \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define printk(...) color_printk(WHITE, BLACK, __VA_ARGS__)

#include "types.h"
#include "types/stdarg.h"

void color_printk(size_t fcolor, size_t bcolor, const char *fmt, ...);

void kmsg_putc(char c);
void kmsg_write(const char *s);
int kmsg_getc(void);

void logk(const char *str);
void logkf(char *fmt, ...);
