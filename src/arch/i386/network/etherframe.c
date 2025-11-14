#include "etherframe.h"
#include "kmalloc.h"
#include "pcnet.h"

extern uint8_t mac0, mac1, mac2, mac3, mac4, mac5;
// 以太网帧
void ether_frame_provider_send(uint64_t dest_mac, uint16_t type, uint8_t *buffer, uint32_t size) {
    uint8_t *buffer2 = (uint8_t *)kmalloc(sizeof(struct EthernetFrame_head) + size +
                                          sizeof(struct EthernetFrame_tail));
    struct EthernetFrame_head *header = (struct EthernetFrame_head *)buffer2;
    struct EthernetFrame_tail *tailer =
        (struct EthernetFrame_tail *)(buffer2 + sizeof(struct EthernetFrame_head) + size);
    header->dest_mac[0] = (uint8_t)dest_mac;
    header->dest_mac[1] = (uint8_t)(dest_mac >> 8);
    header->dest_mac[2] = (uint8_t)(dest_mac >> 16);
    header->dest_mac[3] = (uint8_t)(dest_mac >> 24);
    header->dest_mac[4] = (uint8_t)(dest_mac >> 32);
    header->dest_mac[5] = (uint8_t)(dest_mac >> 40);
    header->src_mac[0]  = mac0;
    header->src_mac[1]  = mac1;
    header->src_mac[2]  = mac2;
    header->src_mac[3]  = mac3;
    header->src_mac[4]  = mac4;
    header->src_mac[5]  = mac5;
    header->type        = (type << 8) | ((type & 0xff00) >> 8);
    tailer->CRC         = 0;
    uint8_t *src        = buffer;
    uint8_t *dst        = buffer2 + sizeof(struct EthernetFrame_head);
    for (uint32_t i = 0; i < size; i++)
        dst[i] = src[i];
    PcnetSend(buffer2,
              sizeof(struct EthernetFrame_head) + size + sizeof(struct EthernetFrame_tail));
    kfree(buffer2);
}
