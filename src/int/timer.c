#include "timer.h"

void ms_to_timeval(uint64_t ms, struct timeval *tv) {
    tv->tv_sec = ms / 1000;
    tv->tv_usec = (ms % 1000) * 1000; // 转换为微秒保持结构体定义
}