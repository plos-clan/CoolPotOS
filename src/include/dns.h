#ifndef CRASHPOWEROS_DNS_H
#define CRASHPOWEROS_DNS_H

#define DNS_Header_ID 0x2115

#define DNS_TYPE_A 1
#define DNS_TYPE_NS 2
#define DNS_TYPE_MD 3
#define DNS_TYPE_MF 4
#define DNS_TYPE_CNAME 5
#define DNS_TYPE_SOA 6
#define DNS_TYPE_MB 7
#define DNS_TYPE_MG 8
#define DNS_TYPE_MR 9
#define DNS_TYPE_NULL 10
#define DNS_TYPE_WKS 11
#define DNS_TYPE_PTR 12
#define DNS_TYPE_HINFO 13
#define DNS_TYPE_MINFO 14
#define DNS_TYPE_MX 15
#define DNS_TYPE_TXT 16
#define DNS_TYPE_ANY 255

#define DNS_CLASS_INET 1
#define DNS_CLASS_CSNET 2
#define DNS_CLASS_CHAOS 3
#define DNS_CLASS_HESIOD 4
#define DNS_CLASS_ANY 255

#define DNS_PORT 53
#define DNS_SERVER_IP 0x08080808

#include <stdint.h>

struct DNS_Header {
    uint16_t ID;
    uint8_t RD : 1;
    uint8_t AA : 1;
    uint8_t Opcode : 4;
    uint8_t QR : 1;
    uint8_t RCODE : 4;
    uint8_t Z : 3;
    uint8_t RA : 1;
    uint8_t TC : 1;
    uint16_t QDcount;
    uint16_t ANcount;
    uint16_t NScount;
    uint16_t ARcount;
    uint8_t reserved;
} __attribute__((packed));

struct DNS_Question {
    uint16_t type;
    uint16_t Class;
} __attribute__((packed));

struct DNS_Answer {
    uint32_t name : 24;
    uint16_t type;
    uint16_t Class;
    uint32_t TTL;
    uint16_t RDlength;
    uint8_t reserved;
    uint8_t RData[0];
} __attribute__((packed));

void dns_handler(void *base);

#endif
