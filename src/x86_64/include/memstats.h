#pragma once

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
