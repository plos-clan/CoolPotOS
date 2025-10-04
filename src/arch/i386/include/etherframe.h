#pragma once

#include "cptype.h"

struct EthernetFrame_head {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t type;
} __attribute__((packed));

struct EthernetFrame_tail {
    uint32_t CRC; // 这里可以填写为0，网卡会自动计算
};

void ether_frame_provider_send(uint64_t dest_mac, uint16_t type, uint8_t *buffer, uint32_t size);