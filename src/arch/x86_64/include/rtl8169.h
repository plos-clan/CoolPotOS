#pragma once

#define TX_DESC_COUNT 4
#define RX_DESC_COUNT 4

#include "ctype.h"

typedef struct {
    uint32_t MAC[2];     // 0x00 - 0x07: MAC 地址 (前 6 字节有效)
    uint32_t MAR[2];     // 0x08 - 0x0F: 多播地址过滤器
    uint32_t TxDesc;     // 0x20: TX 描述符指针
    uint32_t RxDesc;     // 0x24: RX 描述符指针
    uint16_t IntrStatus; // 0x3E: 中断状态
    uint16_t IntrMask;   // 0x3C: 中断屏蔽
    uint8_t  Command;    // 0x37: 命令寄存器
    uint32_t RxConfig;   // 0x44: RX 配置寄存器
    uint32_t TxConfig;   // 0x40: TX 配置寄存器
} __attribute__((packed)) RTL8169_Regs;

typedef struct {
    uint64_t addr;  // 物理地址
    uint32_t len;   // 长度
    uint32_t flags; // 标志
} __attribute__((packed)) RTL8169_Desc;

void rtl8169_setup();
