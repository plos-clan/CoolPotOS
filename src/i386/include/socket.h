#pragma once

#define MAX_SOCKET_NUM 256
#define SOCKET_ALLOC   -1
#define SOCKET_FREE    0

#include "ctypes.h"

struct Socket {
    // 函数格式
    int (*Connect)(struct Socket *socket);                             // TCP
    void (*Disconnect)(struct Socket *socket);                         // TCP
    void (*Listen)(struct Socket *socket);                             // TCP
    void (*Send)(struct Socket *socket, uint8_t *data, uint32_t size); // TCP/UDP
    void (*Handler)(struct Socket *socket, void *base);                // TCP/UDP
    // TCP/UDP
    uint32_t remoteIP;
    uint16_t remotePort;
    uint32_t localIP;
    uint16_t localPort;
    uint8_t  state;
    uint8_t  protocol;
    // TCP
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t MSS;
    int      flag; // 1 有包 0 没包
    int      size;
    char    *buf;
} __attribute__((packed));

void           socket_init();
struct Socket *Socket_Find(uint32_t dstIP, uint16_t dstPort, uint32_t srcIP, uint16_t srcPort,
                           uint8_t protocol);