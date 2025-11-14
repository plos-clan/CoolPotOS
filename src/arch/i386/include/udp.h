#pragma once

#define UDP_PROTOCOL 17

#include "cptype.h"

struct UDPMessage {
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t length;
    uint16_t checkSum;
} __attribute__((packed));

uint8_t *UDP_Packet(uint16_t dest_port, uint16_t src_port, uint8_t *data, uint32_t size);
void     udp_provider_send(uint32_t destip, uint32_t srcip, uint16_t dest_port, uint16_t src_port,
                           uint8_t *data, uint32_t size);
void     udp_handler(void *base);