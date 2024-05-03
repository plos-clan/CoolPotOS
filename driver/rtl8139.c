/*
 * rtl8139.c
 * Realtek rtl8139 网卡驱动
 * Copyright (C) zhouzhihao 2023
 */

#include "../include/rtl8139.h"

static uint8_t bus, dev, func;
static uint32_t io_base;
static uint8_t *sendBuffer[4];
static uint8_t *recvBuffer;
static uint8_t currentSendBuffer;

extern uint8_t mac0, mac1, mac2, mac3, mac4, mac5;
