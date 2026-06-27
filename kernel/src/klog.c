#include "klog.h"
#include "krlibc.h"
#include "term/kprint.h"
#include "driver/device.h"
#include "types/stdarg.h"

/*
 * CoolPotOS 内核日志系统实现
 * 支持环形缓冲区、日志级别过滤、串口回退、模块标签
 */

static klog_ring_t   g_klog_ring;
static klog_level_t  g_min_level = KLOG_INFO;  /* 默认 INFO 级别 */
static bool          g_serial_output = true;   /* 默认启用串口输出 */
static bool          g_initialized = false;

/* 日志级别名称 */
static const char *level_names[] = {
    [KLOG_DEBUG] = "DEBUG",
    [KLOG_INFO]  = "INFO",
    [KLOG_WARN]  = "WARN",
    [KLOG_ERROR] = "ERROR",
    [KLOG_FATAL] = "FATAL",
};

/* 日志级别颜色 */
static const int level_colors[] = {
    [KLOG_DEBUG] = CYAN,
    [KLOG_INFO]  = WHITE,
    [KLOG_WARN]  = YELLOW,
    [KLOG_ERROR] = RED,
    [KLOG_FATAL] = MAGENTA,
};

void klog_init(void) {
    memset(&g_klog_ring, 0, sizeof(g_klog_ring));
    g_klog_ring.max_entries = KLOG_RING_SIZE / sizeof(klog_entry_t);
    g_klog_ring.lock = SPIN_INIT;
    g_initialized = true;

    klog_info("KLOG", "Kernel logging system initialized (level=%s, serial=%s)",
              level_names[g_min_level], g_serial_output ? "on" : "off");
}

void klog_set_level(klog_level_t level) {
    if (level < KLOG_LEVEL_MAX)
        g_min_level = level;
}

klog_level_t klog_get_level(void) {
    return g_min_level;
}

void klog_set_serial_output(bool enable) {
    g_serial_output = enable;
}

void klog_write(klog_level_t level, const char *tag, const char *fmt, ...) {
    if (!g_initialized) return;

    /* 级别过滤 */
    if (level < g_min_level) return;

    /* 截断标签 */
    char safe_tag[KLOG_TAG_MAX];
    if (tag) {
        strncpy(safe_tag, tag, KLOG_TAG_MAX - 1);
        safe_tag[KLOG_TAG_MAX - 1] = '\0';
    } else {
        safe_tag[0] = '\0';
    }

    /* 格式化消息 */
    char msg_buf[KLOG_MAX_MSG];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, KLOG_MAX_MSG, fmt, args);
    va_end(args);

    /* 写入环形缓冲区 */
    spin_lock(g_klog_ring.lock);
    klog_entry_t *entry = &g_klog_ring.entries[g_klog_ring.head];
    entry->timestamp = 0; /* TODO: 使用系统滴答 */
    entry->level = level;
    strncpy(entry->tag, safe_tag, KLOG_TAG_MAX - 1);
    strncpy(entry->msg, msg_buf, KLOG_MAX_MSG - 1);
    entry->tag[KLOG_TAG_MAX - 1] = '\0';
    entry->msg[KLOG_MAX_MSG - 1] = '\0';

    g_klog_ring.head = (g_klog_ring.head + 1) % g_klog_ring.max_entries;
    if (g_klog_ring.count < g_klog_ring.max_entries)
        g_klog_ring.count++;
    spin_unlock(g_klog_ring.lock);

    /* 终端输出 */
    const device_t *tty = device_find(DEV_TTY, 0);
    if (tty) {
        char line[KLOG_MAX_MSG + 64];
        int off = snprintf(line, sizeof(line), "[%s] %s: %s\n",
                          level_names[level], safe_tag, msg_buf);
        if (off > 0) device_write(tty->dev, line, 0, off);
    }

    /* 串口输出 */
    if (g_serial_output) {
        extern void write_serial(const char ch);
        char line[KLOG_MAX_MSG + 64];
        snprintf(line, sizeof(line), "[%s] %s: %s\n\r",
                level_names[level], safe_tag, msg_buf);
        for (const char *p = line; *p; p++)
            write_serial(*p);
    }

    /* 致命错误后停止系统 */
    if (level == KLOG_FATAL) {
        extern void arch_close_interrupt(void);
        extern void arch_wait_for_interrupt(void);
        arch_close_interrupt();
        while (1) arch_wait_for_interrupt();
    }
}

const klog_entry_t *klog_get_entry(size_t index) {
    if (index >= g_klog_ring.count) return NULL;
    size_t real_idx = (g_klog_ring.head + g_klog_ring.max_entries - g_klog_ring.count + index)
                      % g_klog_ring.max_entries;
    return &g_klog_ring.entries[real_idx];
}

size_t klog_get_count(void) {
    return g_klog_ring.count;
}

void klog_flush(void) {
    spin_lock(g_klog_ring.lock);
    /* 环形缓冲区已就绪，无需额外刷新 */
    spin_unlock(g_klog_ring.lock);
}