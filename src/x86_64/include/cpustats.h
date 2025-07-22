#pragma once

#define IA32_MPERF         0x0E7
#define IA32_APERF         0x0E8
#define IA32_PLATFORM_INFO 0xCE

#include "ctype.h"

uint64_t read_tsc();

/* 当前 CPU 核心非节能模式下运行的周期数 */
uint64_t read_mpc();

/* 当前 CPU 核心在实际运行过程中累积的周期数 */
uint64_t read_apfcc();

/* 当前 CPU 基准频率 */
uint64_t get_cpu_base_freq_hz();

/* 当前 CPU 的实时频率 (会有 10 ms 的延迟开销) */
uint64_t estimate_actual_freq();
