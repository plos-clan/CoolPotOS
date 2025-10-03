#pragma once

#include "ctype.h"

/**
 * @brief 播放声音
 * @param n 频率
 * @param d 时常(毫秒)
 */
void beepX(uint32_t n, uint32_t d);

/**
 * @brief 播放默认频率的10毫秒声音
 */
void beep();
