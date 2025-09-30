#pragma once

#define KILO_SHIFT 10
#define MEGA_SHIFT 20
#define GIGA_SHIFT 30

#define KILO_FACTOR (1UL << KILO_SHIFT) // 1024
#define MEGA_FACTOR (1UL << MEGA_SHIFT) // 1048576
#define GIGA_FACTOR (1UL << GIGA_SHIFT) // 1073741824

#include "ctype.h"

/* 硬件保留的内存 包括显存/设备MMIO/ACPI缓存等 */
uint64_t get_reserved_memory();

/* 机器总内存大小 (包括保留内存, 损坏内存等) */
uint64_t get_all_memory();

/* 未使用的内存 */
uint64_t get_available_memory();

/* 由内核/用户程序/驱动等占用的内存 */
uint64_t get_used_memory();

/* 机器损坏的内存 */
uint64_t get_bad_memory();

/* 应用程序已提交的内存 */
uint64_t get_commit_memory();

/**
 * 解析如 1G 260M 24K 1024 字样的大小写法
 * @param s 大小助记符
 * @return 返回解析后的大小(-1 解析失败)
 */
uint64_t mem_parse_size(uint8_t *s);
