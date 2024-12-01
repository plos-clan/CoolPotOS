#include "udp.h"
#include "krlibc.h"
#include "kmalloc.h"
#include "dhcp.h"
#include "ipv4.h"
#include "etherframe.h"
#include "dns.h"
#include "socket.h"
#include "arp.h"

uint8_t *UDP_Packet(uint16_t dest_port, uint16_t src_port, uint8_t *data, uint32_t size) {
    struct UDPMessage *res =
            (struct UDPMessage *)kmalloc(size + sizeof(struct UDPMessage));
    uint8_t *dat = (uint8_t *)res;
    memcpy(dat + sizeof(struct UDPMessage), data, size);
    res->srcPort = ((src_port & 0xff00) >> 8) | ((src_port & 0x00ff) << 8);
    res->dstPort = ((dest_port & 0xff00) >> 8) | ((dest_port & 0x00ff) << 8);
    res->length = size + sizeof(struct UDPMessage);
    res->length = ((res->length & 0xff00) >> 8) | ((res->length & 0x00ff) << 8);
    res->checkSum = 0;
    return (uint8_t *)res;
}

void udp_provider_send(uint32_t destip, uint32_t srcip, uint16_t dest_port,
                       uint16_t src_port, uint8_t *data, uint32_t size) {
    if (destip == 0xffffffff) {
        IPV4ProviderSend(17, 0xffffffffffff, destip, srcip,
                         UDP_Packet(dest_port, src_port, data, size),
                         size + sizeof(struct UDPMessage));
        return;
    }
    IPV4ProviderSend(17, IPParseMAC(destip), destip, srcip,
                     UDP_Packet(dest_port, src_port, data, size),
                     size + sizeof(struct UDPMessage));
}

void udp_handler(void *base) {
    // printk("UDP Packet:\n");
    struct IPV4Message *ipv4 =
            (struct IPV4Message *)(base + sizeof(struct EthernetFrame_head));
    struct UDPMessage *udp =
            (struct UDPMessage *)(base + sizeof(struct EthernetFrame_head) +
                                  sizeof(struct IPV4Message));
    if (swap16(udp->srcPort) == DHCP_SERVER_PORT) { // DHCP给我们分配IP
        dhcp_handler(base);
    } else if (swap32(ipv4->srcIP) == DNS_SERVER_IP &&
               swap16(udp->srcPort) == DNS_PORT) {
        dns_handler(base);
    } else {
        // Socket
        struct Socket *socket =
                Socket_Find(swap32(ipv4->srcIP), swap16(udp->srcPort),
                            swap32(ipv4->dstIP), swap16(udp->dstPort), UDP_PROTOCOL);
        if (socket == -1) {
            return;
        }
        if (socket->Handler != NULL) {
            socket->Handler(socket, base);
        }
    }
}