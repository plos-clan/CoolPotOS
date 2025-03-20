#pragma once

#define IP_PROTOCOL 0x0800
#define MTU 1500
#define IP_MF 13
#define IP_DF 14
#define IP_OFFSET 0

#include "ctypes.h"

struct IPV4Message {
    uint8_t headerLength : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t totalLength;
    uint16_t ident;
    uint16_t flagsAndOffset;
    uint8_t timeToLive;
    uint8_t protocol;
    uint16_t checkSum;
    uint32_t srcIP;
    uint32_t dstIP;
} __attribute__((packed));

void IPV4ProviderSend(uint8_t protocol, uint64_t dest_mac, uint32_t dest_ip,
                      uint32_t src_ip, uint8_t *data, uint32_t size);
uint16_t CheckSum(uint16_t *data, uint32_t size);
uint32_t IP2UINT32_T(uint8_t *ip);