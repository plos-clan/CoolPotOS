#pragma once

#include "types.h"

/*
 * CoolPotOS 内核安全模块
 * 提供栈保护、输入验证、内存安全检查、地址随机化
 */

/* ---- 栈保护 (Stack Canary) ---- */

/* 初始化栈保护 (每次启动随机生成 canary 值) */
void ksecure_stack_init(void);

/* 获取当前栈 canary 值 */
uint64_t ksecure_get_canary(void);

/* 验证栈 canary 是否被破坏 */
bool ksecure_check_canary(uint64_t canary);

/* ---- 输入验证 ---- */

/* 验证指针是否在内核地址空间内 */
bool ksecure_is_kernel_ptr(const void *ptr);

/* 验证用户空间指针是否合法 */
bool ksecure_is_user_ptr(const void *ptr, size_t len);

/* 验证字符串长度是否安全 */
bool ksecure_str_is_safe(const char *str, size_t max_len);

/* 安全字符串复制 (带边界检查) */
size_t ksecure_strncpy(char *dst, const char *src, size_t dst_size);

/* ---- 内存安全 ---- */

/* 验证内存范围是否可读 */
bool ksecure_mem_is_readable(const void *addr, size_t size);

/* 验证内存范围是否可写 */
bool ksecure_mem_is_writable(const void *addr, size_t size);

/* 清零敏感内存 (防止编译器优化掉) */
void ksecure_memzero(void *ptr, size_t size);

/* ---- 地址空间随机化 (ASLR) ---- */

/* 初始化 ASLR */
void ksecure_aslr_init(void);

/* 获取随机偏移 (用于栈/堆/MMAP 随机化) */
uint64_t ksecure_aslr_offset(void);

/* ---- 审计日志 ---- */

/* 记录安全事件 */
void ksecure_audit_log(const char *event, const char *detail);

/* 获取安全事件计数 */
uint64_t ksecure_get_audit_count(void);