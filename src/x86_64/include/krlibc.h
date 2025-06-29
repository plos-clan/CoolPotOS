#pragma once

/**
 * 定义CP_Kernel内核的各种属性
 */
#define KERNEL_NAME           "CP_Kernel-x86_64-0.2.9" // 内核编号
#define MAX_CPU               256                      // 最大支持CPU核心数 256
#define KERNEL_HEAP_START     0xffff900000000000       // 内核堆起始地址
#define KERNEL_HEAP_SIZE      0x2000000                // 内核堆大小 32MB
#define SMALL_STACK_SIZE      8192                     // fork 精简栈大小(byte)
#define STACK_SIZE            32768                    // 栈大小(byte)
#define KERNEL_ST_SZ          131072                   // 增强栈大小 128k
#define BIG_USER_STACK        999424                   // 用户栈大小，要对齐到页
#define MAX_WAIT_INDEX        1000000                  // 阻塞最大循环数
#define KERNEL_AREA_MEM       0xf000000000000000       // 内核地址空间起始
#define USER_MMAP_START       0x0000400000000000UL     // 用户堆映射起始地址
#define USER_MMAP_END         0x0000700000000000UL     // 用户堆映射结束地址
#define MAX_SIGNALS           64                       // 最大支持信号个数
#define EHDR_START_ADDR       0x0000300000000000       // ELF头起始地址
#define INTERPRETER_EHDR_ADDR 0x0000200000000000       // 链接器ELF头起始地址
#define INTERPRETER_BASE_ADDR 0x0000100000000000       // 链接器基址起始地址
#define DEVFS_REGISTER_ID     0                        // 设备文件系统注册ID
#define MODFS_REGISTER_ID     1                        // 模块文件系统注册ID
#define TMPFS_REGISTER_ID     2                        // 临时文件系统注册ID
#define LAPIC_TIMER_SPEED     50                       // LAPIC定时器速度(单位: Hz)

// 常用工具宏
#define cpu_hlt loop __asm__("hlt")
#define UNUSED(expr)                                                                               \
    do {                                                                                           \
        (void)(expr);                                                                              \
    } while (0)
#define __IRQHANDLER __attribute__((interrupt))

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)

#define waitif(cond)                                                                               \
    ((void)({                                                                                      \
        while (cond) {}                                                                            \
    }))

#define streq(s1, s2)                                                                              \
    ({                                                                                             \
        const char *_s1 = (s1), *_s2 = (s2);                                                       \
        (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                         \
    })

#include "ctype.h"
#include "limits.h"

static inline void empty() {}

void not_null_assets(void *ptr, const char *message); // error_handle.c defined

int memcmp(const void *a_, const void *b_, size_t size);

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *dest, int val, size_t size);

void *memmove(void *dest, const void *src, size_t n);

size_t strlen(const char *str);

char *strcat(char *dest, const char *src);

int strncmp(const char *s1, const char *s2, size_t n);

char *strcpy(char *dest, const char *src);

int strcmp(const char *s1, const char *s2);

int64_t strtol(const char *str, char **endptr, int base);

char *strdup(const char *str);

char *strchrnul(const char *s, int c);

int isspace(int c);

char *strchr(const char *s, int c);

char *strncpy(char *dest, const char *src, size_t n);

char *strrchr(const char *s, int c);

char *strtok(char *str, const char *delim);

char *pathacat(char *p1, char *p2);

int cmd_parse(const char *cmd_str, char **argv, char token);

char *normalize_path(const char *path);

static inline bool are_interrupts_enabled() {
    uint64_t rflags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(rflags));
    return (rflags & (1 << 9)) != 0;
}

static inline char *LeadingWhitespace(char *beg, char *end) {
    while (end > beg && *--end <= 0x20) {
        *end = 0;
    }
    while (beg < end && *beg <= 0x20) {
        beg++;
    }
    return beg;
}
