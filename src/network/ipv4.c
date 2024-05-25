#include "../include/ipv4.h"
#include "../include/memory.h"
#include "../include/common.h"

static uint16_t ident = 0;

void IPV4ProviderSend(uint8_t protocol, uint64_t dest_mac, uint32_t dest_ip,
                      uint32_t src_ip, uint8_t *data, uint32_t size) {
    struct IPV4Message *res =
            (struct IPV4Message *)kmalloc(sizeof(struct IPV4Message) + size);
    uint8_t *dat = (uint8_t *)res;
    memcpy(dat + sizeof(struct IPV4Message), data, size);
    res->version = 4;
    res->headerLength = sizeof(struct IPV4Message) / 4;
    res->tos = 0;
    res->ident = ident;
    res->timeToLive = 64;
    res->protocol = protocol;
    res->dstIP = ((dest_ip << 24) & 0xff000000) | ((dest_ip << 8) & 0x00ff0000) |
                 ((dest_ip >> 8) & 0xff00) | ((dest_ip >> 24) & 0xff);
    res->srcIP = ((src_ip << 24) & 0xff000000) | ((src_ip << 8) & 0x00ff0000) |
                 ((src_ip >> 8) & 0xff00) | ((src_ip >> 24) & 0xff);
    if (sizeof(struct IPV4Message) + size <= MTU) {
        res->totalLength = swap16(sizeof(struct IPV4Message) + size);
        res->flagsAndOffset = 0;
        res->checkSum = 0;
        res->checkSum = CheckSum((uint16_t *)dat, sizeof(struct IPV4Message));
        ether_frame_provider_send(dest_mac, 0x0800, dat,
                                  sizeof(struct IPV4Message) + size);
    } else {
        int offset = 0;
        uint8_t *dat1 = (uint8_t *)kmalloc(MTU);
        for (int i = 0; i * (MTU - sizeof(struct IPV4Message)) <= size; i++) {
            if (i * (MTU - sizeof(struct IPV4Message)) >=
                size - (MTU - sizeof(struct IPV4Message))) {
                res->totalLength =
                        swap16(size - i * (MTU - sizeof(struct IPV4Message)) +
                               sizeof(struct IPV4Message));
                res->flagsAndOffset = offset << IP_OFFSET;
                res->flagsAndOffset = swap16(res->flagsAndOffset);
                res->checkSum = 0;
                res->checkSum = CheckSum((uint16_t *)dat, sizeof(struct IPV4Message));
                memcpy((void *)dat1, (void *)res, sizeof(struct IPV4Message));
                memcpy((void *)(dat1 + sizeof(struct IPV4Message)),
                       (void *)(data + i * (MTU - sizeof(struct IPV4Message))),
                       size - i * (MTU - sizeof(struct IPV4Message)));
                // printk("ip:%08x,%08x
                // size:%d\nMF:0\noffset:%d\n",swap32(res->srcIP),swap32(res->dstIP),swap16(res->totalLength),(swap16(res->flagsAndOffset)
                // >> 3));
                ether_frame_provider_send(dest_mac, 0x0800, dat1,
                                          size - i * (MTU - sizeof(struct IPV4Message)) +
                                          sizeof(struct IPV4Message));
            } else {
                res->totalLength = swap16(MTU);
                res->flagsAndOffset = (offset << IP_OFFSET) | (1 << IP_MF);
                res->flagsAndOffset = swap16(res->flagsAndOffset);
                res->checkSum = 0;
                res->checkSum = CheckSum((uint16_t *)dat, sizeof(struct IPV4Message));
                memcpy((void *)dat1, (void *)res, sizeof(struct IPV4Message));
                memcpy((void *)(dat1 + sizeof(struct IPV4Message)),
                       (void *)(data + i * (MTU - sizeof(struct IPV4Message))),
                       MTU - sizeof(struct IPV4Message));
                // printk("ip:%08x,%08x
                // size:%d\nMF:1\noffset:%d\n",swap32(res->srcIP),swap32(res->dstIP),swap16(res->totalLength),(swap16(res->flagsAndOffset)
                // >> 3));
                ether_frame_provider_send(dest_mac, 0x0800, dat1, MTU);
            }
            offset += (MTU - sizeof(struct IPV4Message)) / 8;
        }
        kfree((void *)dat1);
    }

    kfree(dat);
    ident++;
    return;
}
uint16_t CheckSum(uint16_t *data, uint32_t size) {
    uint32_t tmp = 0;
    for (int i = 0; i < size / 2; i++) {
        tmp += ((data[i] & 0xff00) >> 8) | ((data[i] & 0x00ff) << 8);
    }
    if (size % 2)
        tmp += ((uint16_t)((char *)data)[size - 1]) << 8;
    while (tmp & 0xffff0000)
        tmp = (tmp & 0xffff) + (tmp >> 16);
    return ((~tmp & 0xff00) >> 8) | ((~tmp & 0x00ff) << 8);
}
uint32_t IP2UINT32_T(uint8_t *ip) {
    uint8_t ip0, ip1, ip2, ip3;
    ip0 = strtol(ip, '.', 10);
    uint8_t t = ip0;
    while (t >= 10) {
        t /= 10;
        ip++;
    }
    ip1 = strtol(ip + 2, '.', 10);
    t = ip1;
    while (t >= 10) {
        t /= 10;
        ip++;
    }
    ip2 = strtol(ip + 4, '.', 10);
    t = ip2;
    while (t >= 10) {
        t /= 10;
        ip++;
    }
    ip3 = strtol(ip + 6, NULL, 10);
    return (uint32_t)((ip0 << 24) | (ip1 << 16) | (ip2 << 8) | ip3);
}
