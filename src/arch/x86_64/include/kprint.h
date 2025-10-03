#pragma once

#include "cptype.h"
#include "types/stdarg.h"

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

void color_printk(size_t fcolor, size_t bcolor, const char *fmt, ...);

#define printk(...) color_printk(WHITE, BLACK, __VA_ARGS__)

void init_print_lock();

/**
 * 内核模块用链接输出
 * @param fmt
 * @param ...
 */
void cp_printf(const char *fmt, ...);
