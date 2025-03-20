#include "ctype.h"
#include <stdarg.h>

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
        color_printk(GREEN, BLACK, "SUCCESS");                                                     \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kinfo(...)                                                                                 \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(CYAN, BLACK, " INFO ");                                                       \
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
        color_printk(YELLOW, BLACK, " WARN ");                                                     \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

#define kerror(...)                                                                                \
    do {                                                                                           \
        printk("[");                                                                               \
        color_printk(RED, BLACK, "ERROR ");                                                        \
        printk("]: ");                                                                             \
        printk(__VA_ARGS__);                                                                       \
        printk("\n");                                                                              \
    } while (0)

void color_printk(size_t fcolor, size_t bcolor, const char *fmt, ...);

#define printk(...) color_printk(WHITE, BLACK, __VA_ARGS__)

/**
 * 中断专用输出
 * warning: 该函数会强行解开打印锁并越过原子缓冲队列直接向终端打印, 谨慎使用
 * @param fmt
 * @param ...
 */
void printe(const char *fmt, ...);
