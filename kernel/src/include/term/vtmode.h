#pragma once

#define KDGETMODE   0x4B3B // 获取终端模式命令
#define KDSETMODE   0x4B3A // 设置终端模式命令
#define KD_TEXT     0x00   // 文本模式
#define KD_GRAPHICS 0x01   // 图形模式

#define KDGKBMODE   0x4B44 /* gets current keyboard mode */
#define KDSKBMODE   0x4B45 /* sets current keyboard mode */
#define K_RAW       0x00   // 原始模式（未处理扫描码）
#define K_XLATE     0x01   // 转换模式（生成ASCII）
#define K_MEDIUMRAW 0x02   // 中等原始模式
#define K_UNICODE   0x03   // Unicode模式

#define VT_OPENQRY 0x5600 /* get next available vt */
#define VT_GETMODE 0x5601 /* get mode of active vt */
#define VT_SETMODE 0x5602

#define VT_GETSTATE 0x5603
#define VT_SENDSIG  0x5604

#define VT_ACTIVATE   0x5606 /* make vt active */
#define VT_WAITACTIVE 0x5607 /* wait for vt active */

#define VT_AUTO    0x00 // 自动切换模式
#define VT_PROCESS 0x01 // 进程控制模式

#include "types.h"

struct vt_state {
    uint16_t v_active; // 活动终端号
    uint16_t v_state;  // 终端状态标志
};

typedef struct vt_mode {
    char mode;    // 终端模式
    char waitv;   // 垂直同步
    short relsig; // 释放信号
    short acqsig; // 获取信号
    short frsig;  // 强制释放信号
} vt_mode;
