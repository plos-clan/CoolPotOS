#pragma once

#include "types.h"
#include "lock.h"

/*
 * CoolPotOS 内核日志系统
 * 提供环形缓冲区日志、日志级别过滤、模块标签、串口回退
 */

/* 日志级别 */
typedef enum {
    KLOG_DEBUG = 0,   /* 调试信息 */
    KLOG_INFO  = 1,   /* 一般信息 */
    KLOG_WARN  = 2,   /* 警告 */
    KLOG_ERROR = 3,   /* 错误 */
    KLOG_FATAL = 4,   /* 致命错误 */
    KLOG_LEVEL_MAX
} klog_level_t;

/* 日志环形缓冲区配置 */
#define KLOG_RING_SIZE    4096     /* 环形缓冲区大小 (字节) */
#define KLOG_MAX_MSG      256      /* 单条日志最大长度 */
#define KLOG_TAG_MAX      16       /* 模块标签最大长度 */

/* 日志条目 */
typedef struct klog_entry {
    uint64_t     timestamp;        /* 时间戳 (系统滴答) */
    klog_level_t level;            /* 日志级别 */
    char         tag[KLOG_TAG_MAX];/* 模块标签 */
    char         msg[KLOG_MAX_MSG];/* 日志消息 */
} klog_entry_t;

/* 日志环形缓冲区 */
typedef struct klog_ring {
    klog_entry_t entries[KLOG_RING_SIZE / sizeof(klog_entry_t)];
    size_t       head;             /* 写指针 */
    size_t       count;           /* 条目数 */
    size_t       max_entries;     /* 最大条目数 */
    spin_t       lock;
} klog_ring_t;

/* ---- API ---- */
void klog_init(void);

/* 设置全局日志级别 (低于此级别的日志将被过滤) */
void klog_set_level(klog_level_t level);
klog_level_t klog_get_level(void);

/* 核心日志函数 */
void klog_write(klog_level_t level, const char *tag, const char *fmt, ...);

/* 便捷宏 */
#define klog_debug(tag, ...) klog_write(KLOG_DEBUG, tag, __VA_ARGS__)
#define klog_info(tag, ...)  klog_write(KLOG_INFO,  tag, __VA_ARGS__)
#define klog_warn(tag, ...)  klog_write(KLOG_WARN,  tag, __VA_ARGS__)
#define klog_error(tag, ...) klog_write(KLOG_ERROR, tag, __VA_ARGS__)
#define klog_fatal(tag, ...) klog_write(KLOG_FATAL, tag, __VA_ARGS__)

/* 环形缓冲区访问 */
const klog_entry_t *klog_get_entry(size_t index);
size_t klog_get_count(void);

/* 将日志输出到串口 (用于早期调试) */
void klog_set_serial_output(bool enable);

/* 刷新日志到终端 */
void klog_flush(void);