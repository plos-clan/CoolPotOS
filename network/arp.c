#include "../include/arp.h"
#include "../include/etherframe.h"
#include "../include/memory.h"

uint8_t ARP_flags = 1;
uint64_t ARP_mac_address[MAX_ARP_TABLE];
uint32_t ARP_ip_address[MAX_ARP_TABLE];
uint32_t ARP_write_pointer = 0;

uint8_t *ARP_Packet(uint64_t dest_mac, uint32_t dest_ip, uint64_t src_mac,
                    uint32_t src_ip, uint16_t command) {
    struct ARPMessage *res =
            (struct ARPMessage *)kmalloc(sizeof(struct ARPMessage));
    res->hardwareType = 0x0100;
    res->protocol = 0x0008;
    res->hardwareAddressSize = 6;
    res->protocolAddressSize = 4;
    res->command = ((command & 0xff00) >> 8) | ((command & 0x00ff) << 8);
    res->dest_mac[0] = (uint8_t)dest_mac;
    res->dest_mac[1] = (uint8_t)(dest_mac >> 8);
    res->dest_mac[2] = (uint8_t)(dest_mac >> 16);
    res->dest_mac[3] = (uint8_t)(dest_mac >> 24);
    res->dest_mac[4] = (uint8_t)(dest_mac >> 32);
    res->dest_mac[5] = (uint8_t)(dest_mac >> 40);
    res->dest_ip = ((dest_ip << 24) & 0xff000000) |
                   ((dest_ip << 8) & 0x00ff0000) | ((dest_ip >> 8) & 0xff00) |
                   ((dest_ip >> 24) & 0xff);
    res->src_mac[0] = (uint8_t)src_mac;
    res->src_mac[1] = (uint8_t)(src_mac >> 8);
    res->src_mac[2] = (uint8_t)(src_mac >> 16);
    res->src_mac[3] = (uint8_t)(src_mac >> 24);
    res->src_mac[4] = (uint8_t)(src_mac >> 32);
    res->src_mac[5] = (uint8_t)(src_mac >> 40);
    res->src_ip = ((src_ip << 24) & 0xff000000) | ((src_ip << 8) & 0x00ff0000) |
                  ((src_ip >> 8) & 0xff00) | ((src_ip >> 24) & 0xff);
    return (uint8_t *)res;
}

uint64_t IPParseMAC(uint32_t dstIP) {
    extern uint8_t ARP_flags;
    extern uint32_t ARP_write_pointer;
    extern uint64_t ARP_mac_address[MAX_ARP_TABLE];
    extern uint32_t ARP_ip_address[MAX_ARP_TABLE];
    extern uint32_t ip;
    extern uint8_t mac0;
    extern uint32_t gateway;
    //extern struct TIMERCTL timerctl;
    if ((dstIP & 0xffffff00) != (ip & 0xffffff00)) {
        dstIP = gateway;
    }
    for (int i = 0; i != ARP_write_pointer; i++) {
        if (dstIP == ARP_ip_address[i]) {
            return ARP_mac_address[i];
        }
    }
    ARP_flags = 1;
    //printk("send\n");
    ether_frame_provider_send(
            0xffffffffffff, 0x0806,
            ARP_Packet(0xffffffffffff, dstIP, *(uint64_t *)&mac0, ip, 1),
            sizeof(struct ARPMessage));
    //printk("ok\n");
    /*
    uint32_t time = timerctl.count;
    while (ARP_flags) {
        if (timerctl.count - time > ARP_WAITTIME) {
            return -1;
        }
    }
     */
    return ARP_mac_address[ARP_write_pointer - 1];
}
