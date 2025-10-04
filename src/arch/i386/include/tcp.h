#pragma once

#define TCP_PROTOCOL         6
#define TCP_CONNECT_WAITTIME 10000
#define MSS_Default          1460
#define TCP_SEG_WAITTIME     100

#define SOCKET_TCP_CLOSED       1
#define SOCKET_TCP_LISTEN       2
#define SOCKET_TCP_SYN_SENT     3
#define SOCKET_TCP_SYN_RECEIVED 4
#define SOCKET_TCP_ESTABLISHED  5
#define SOCKET_TCP_FIN_WAIT1    6
#define SOCKET_TCP_FIN_WAIT2    7
#define SOCKET_TCP_CLOSING      8
#define SOCKET_TCP_TIME_WAIT    9
#define SOCKET_TCP_CLOSE_WAIT   10
#define SOCKET_TCP_LAST_ACK     11

#include "cptype.h"

struct TCPPesudoHeader {
    uint32_t srcIP;
    uint32_t dstIP;
    uint16_t protocol;
    uint16_t totalLength;
} __attribute__((packed));

struct TCPMessage {
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint8_t  reserved     : 4;
    uint8_t  headerLength : 4;
    uint8_t  FIN          : 1;
    uint8_t  SYN          : 1;
    uint8_t  RST          : 1;
    uint8_t  PSH          : 1;
    uint8_t  ACK          : 1;
    uint8_t  URG          : 1;
    uint8_t  ECE          : 1;
    uint8_t  CWR          : 1;
    uint16_t window;
    uint16_t checkSum;
    uint16_t pointer;
    uint32_t options[0];
} __attribute__((packed));

void tcp_provider_send(uint32_t dstIP, uint32_t srcIP, uint16_t dstPort, uint16_t srcPort,
                       uint32_t Sequence, uint32_t ackNum, bool URG, bool ACK, bool PSH, bool RST,
                       bool SYN, bool FIN, bool ECE, bool CWR, uint8_t *data, uint32_t size);
void tcp_handler(void *base);
