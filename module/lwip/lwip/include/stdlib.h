#pragma once

#include <arch/cc.h>
#include <limits.h>

static inline int atoi(const char *str) {
    int i = 0;
    int sign = 1;
    long result = 0; // 使用 long 来检测溢出

    // 跳过前导空白字符
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' ||
           str[i] == '\r' || str[i] == '\f' || str[i] == '\v') {
        i++;
    }

    // 处理可选的正负号
    if (str[i] == '-' || str[i] == '+') {
        sign = (str[i] == '-') ? -1 : 1;
        i++;
    }

    // 转换数字部分
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');

        // 检查溢出
        if (sign == 1 && result > INT_MAX) {
            return INT_MAX;
        } else if (sign == -1 && -result < INT_MIN) {
            return INT_MIN;
        }

        i++;
    }

    return (int)(sign * result);
}
