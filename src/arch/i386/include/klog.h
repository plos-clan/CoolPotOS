#pragma once

#include "cptype.h"

void dlogf(char *fmt, ...); // 驱动信息输出

void klogf(bool isok, char *fmt, ...); // 驱动加载状态 (false:失败 true:成功)

void logkf(char *formet, ...); // 串口输出

void logk(const char *message);

void printk(const char *formet, ...); // 内核标准输出

void k_print(const char *message);
