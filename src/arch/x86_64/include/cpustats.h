#pragma once

#include "cptype.h"

/**
 * 读取当前的时间戳计数器值。
 * @return 返回当前的时间戳计数器值。
 */
uint64_t read_tsc();

/**
 * 获取当前的调度时钟值。
 * @return 返回当前的调度时钟值，单位为纳秒。
 */
size_t sched_clock();

void calibrate_tsc_with_hpet();
