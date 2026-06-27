#include "ksecure.h"
#include "krlibc.h"
#include "term/kprint.h"
#include "klog.h"
#include "lock.h"
#include "arch.h"

/*
 * CoolPotOS 内核安全模块实现
 */

/* ---- 栈保护 ---- */

static uint64_t g_stack_canary = 0;
static bool     g_canary_initialized = false;

void ksecure_stack_init(void) {
    /* 使用 RDTSC 或类似机制生成随机 canary */
    uint64_t seed = 0;

    /* 尝试使用 RDTSC 作为随机种子 */
#if defined(__x86_64__) || defined(__amd64__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    seed = ((uint64_t)hi << 32) | lo;
#else
    /* 回退：使用编译时地址作为熵源 */
    seed = ((uint64_t)&g_stack_canary ^ (uint64_t)&seed);
#endif

    /* 确保 LSB 为 0 (防止字符串泄露) */
    seed = (seed & ~0xFFULL) | 0x00;

    /* 确保至少一个 NULL 字节 (防止字符串溢出泄露) */
    if ((seed & 0xFF00) == 0) seed |= 0x0100;

    g_stack_canary = seed;
    g_canary_initialized = true;

    klog_info("SECURE", "Stack canary initialized: 0x%llx", g_stack_canary);
}

uint64_t ksecure_get_canary(void) {
    if (!g_canary_initialized) ksecure_stack_init();
    return g_stack_canary;
}

bool ksecure_check_canary(uint64_t canary) {
    return canary == g_stack_canary;
}

/* ---- 输入验证 ---- */

bool ksecure_is_kernel_ptr(const void *ptr) {
    if (!ptr) return false;
    uint64_t addr = (uint64_t)ptr;
    /* 内核地址空间: 0xFFFF800000000000 以上 */
    return addr >= 0xFFFF800000000000ULL;
}

bool ksecure_is_user_ptr(const void *ptr, size_t len) {
    if (!ptr) return false;
    uint64_t addr = (uint64_t)ptr;
    uint64_t end  = addr + len;

    /* 用户空间: 低于 0x0000800000000000 */
    /* 检查溢出 */
    if (end < addr) return false;
    return end <= 0x0000800000000000ULL;
}

bool ksecure_str_is_safe(const char *str, size_t max_len) {
    if (!str) return false;
    if (!ksecure_is_kernel_ptr(str) && !ksecure_is_user_ptr(str, 1))
        return false;

    /* 检查字符串是否以 null 结尾 */
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') return true;
    }
    return false;
}

size_t ksecure_strncpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return 0;

    size_t i;
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return i;
}

/* ---- 内存安全 ---- */

bool ksecure_mem_is_readable(const void *addr, size_t size) {
    if (!addr || size == 0) return false;
    uint64_t a = (uint64_t)addr;
    uint64_t end = a + size;
    if (end < a) return false; /* 溢出 */

    /* 基本检查：地址必须在合理范围内 */
    /* 内核地址空间或已映射的用户空间 */
    return (a >= 0xFFFF800000000000ULL) ||
           (end <= 0x0000800000000000ULL);
}

bool ksecure_mem_is_writable(const void *addr, size_t size) {
    /* 与可读相同的基本检查，实际需要检查页表写权限 */
    return ksecure_mem_is_readable(addr, size);
}

void ksecure_memzero(void *ptr, size_t size) {
    if (!ptr || size == 0) return;
    /* 使用 volatile 防止编译器优化掉 */
    volatile char *p = (volatile char *)ptr;
    while (size--) *p++ = 0;
    /* 内存屏障确保写入完成 */
    barrier();
}

/* ---- ASLR ---- */

static uint64_t g_aslr_offset = 0;
static bool     g_aslr_initialized = false;

void ksecure_aslr_init(void) {
#if defined(__x86_64__) || defined(__amd64__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    g_aslr_offset = (((uint64_t)hi << 32) | lo) & 0x1FFFFFFFULL; /* 512MB 范围 */
#else
    g_aslr_offset = 0;
#endif
    g_aslr_offset = (g_aslr_offset & ~0xFFFULL); /* 页对齐 */
    g_aslr_initialized = true;

    klog_info("SECURE", "ASLR initialized, offset: 0x%llx", g_aslr_offset);
}

uint64_t ksecure_aslr_offset(void) {
    if (!g_aslr_initialized) ksecure_aslr_init();
    return g_aslr_offset;
}

/* ---- 审计日志 ---- */

static uint64_t g_audit_count = 0;
static spin_t   g_audit_lock = SPIN_INIT;

void ksecure_audit_log(const char *event, const char *detail) {
    if (!event) return;

    spin_lock(g_audit_lock);
    g_audit_count++;
    spin_unlock(g_audit_lock);

    klog_warn("SECURE", "[AUDIT #%llu] %s: %s", g_audit_count, event, detail ? detail : "");
}

uint64_t ksecure_get_audit_count(void) {
    return g_audit_count;
}