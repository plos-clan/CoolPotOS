#pragma once

#define CARD_VENDOR_ID 0x1022
#define CARD_DEVICE_ID 0x2000
//  为了使用该寄存器，必须将RAP设置为这些值
#define CSR0           0
#define CSR1           1
#define CSR2           2
#define CSR3           3
#define CSR4           4
#define BCR18          18
#define BCR20          20
// 16位I/O端口（或到I/O空间开始的偏移）。
#define APROM0         0x00
#define APROM1         0x01
#define APROM2         0x02
#define APROM3         0x03
#define APROM4         0x04
#define APROM5         0x05
// 16位读写模式下
#define RDP16          0x10
#define RAP16          0x12
#define RESET16        0x14
#define BDP16          0x16
// 32位读写模式下
#define RDP32          0x10
#define RAP32          0x14
#define RESET32        0x18
#define BDP32          0x1c
#define MTU            1500
#define ARP_PROTOCOL   0x806
#define IP_PROTOCOL    0x0800
#define IP_MF          13
#define IP_DF          14
#define IP_OFFSET      0

#include "cptype.h"

struct InitializationBlock {
    uint16_t mode;
    uint8_t  reserved1numSendBuffers;
    uint8_t  reserved2numRecvBuffers;
    uint8_t  mac0, mac1, mac2, mac3, mac4, mac5;
    uint16_t reserved3;
    uint64_t logicalAddress;
    uint32_t recvBufferDescAddress;
    uint32_t sendBufferDescAddress;
} __attribute__((packed));

struct BufferDescriptor {
    uint32_t address;
    uint32_t flags;
    uint32_t flags2;
    uint32_t avail;
} __attribute__((packed));

void pcnet_setup();
void PcnetSend(uint8_t *buffer, int size);
void Recv();
void Card_Recv_Handler(uint8_t *RawData);