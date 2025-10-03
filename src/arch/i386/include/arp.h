#pragma once

#define ARP_PROTOCOL  0x0806
#define MAX_ARP_TABLE 256
#define ARP_WAITTIME  1

#include "ctype.h"

struct ARPMessage {
    uint16_t hardwareType;
    uint16_t protocol;
    uint8_t  hardwareAddressSize;
    uint8_t  protocolAddressSize;
    uint16_t command;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed));

uint64_t IPParseMAC(uint32_t dstIP);
uint8_t *ARP_Packet(uint64_t dest_mac, uint32_t dest_ip, uint64_t src_mac, uint32_t src_ip,
                    uint16_t command);
void     arp_handler(void *base);