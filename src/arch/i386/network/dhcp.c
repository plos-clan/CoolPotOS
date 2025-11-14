#include "dhcp.h"
#include "etherframe.h"
#include "ipv4.h"
#include "klog.h"
#include "kmalloc.h"
#include "krlibc.h"
#include "page.h"
#include "udp.h"

uint32_t gateway, submask, dns, ip, dhcp_ip;

static int fill_dhcp_option(uint8_t *packet, uint8_t code, uint8_t *data, uint8_t len) {
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);

    return len + (sizeof(uint8_t) * 2);
}

static int fill_dhcp_discovery_options(struct DHCPMessage *dhcp) {
    int      len = 0;
    uint32_t req_ip;
    uint8_t  parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER,
                                     MESSAGE_TYPE_DNS, MESSAGE_TYPE_DOMAIN_NAME};
    uint8_t  option;

    option  = DHCP_OPTION_DISCOVER;
    len    += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_DHCP, &option, sizeof(option));
    req_ip  = swap32(0xffffffff);
    len    += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_REQ_IP, (uint8_t *)&req_ip,
                               sizeof(req_ip));
    len    += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_PARAMETER_REQ_LIST,
                               (uint8_t *)&parameter_req_list, sizeof(parameter_req_list));
    option  = 0;
    len    += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_END, &option, sizeof(option));

    return len;
}

static void dhcp_output(struct DHCPMessage *dhcp, uint8_t *mac, int *len) {
    *len += sizeof(struct DHCPMessage);
    memset(dhcp, 0, sizeof(struct DHCPMessage));

    dhcp->opcode = DHCP_BOOTREQUEST;
    dhcp->htype  = DHCP_HARDWARE_TYPE_10_EHTHERNET;
    dhcp->hlen   = 6;
    memcpy(dhcp->chaddr, mac, DHCP_CHADDR_LEN);

    dhcp->magic_cookie = swap32(DHCP_MAGIC_COOKIE);
}

int dhcp_discovery(uint8_t *mac) {
    int                 len  = 0;
    struct DHCPMessage *dhcp = (struct DHCPMessage *)kmalloc(sizeof(struct DHCPMessage));

    len = fill_dhcp_discovery_options(dhcp);
    dhcp_output(dhcp, mac, &len);
    udp_provider_send(0xffffffff, 0x0, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (uint8_t *)dhcp, len);
    return 0;
}

void dhcp_handler(void *base) {
    printk("DHCP\n");
    struct IPV4Message *ipv4 = (struct IPV4Message *)(base + sizeof(struct EthernetFrame_head));
    struct UDPMessage  *udp  = (struct UDPMessage *)(base + sizeof(struct EthernetFrame_head) +
                                                   sizeof(struct IPV4Message));
    struct DHCPMessage *dhcp =
        (struct DHCPMessage *)(base + sizeof(struct EthernetFrame_head) +
                               sizeof(struct IPV4Message) + sizeof(struct UDPMessage));

    printk("%d %d %d\n", dhcp->bp_options[0], dhcp->bp_options[1], dhcp->bp_options[2]);
    if (dhcp->bp_options[0] == 53 && dhcp->bp_options[1] == 1 && dhcp->opcode == 2) {
        printk("DHCP Offer\n");
        ip           = dhcp->yiaddr;
        uint8_t nip1 = ip;
        uint8_t nip2 = ip >> 8;
        uint8_t nip3 = ip >> 16;
        uint8_t nip4 = ip >> 24;
        printk("DHCP: %d.%d.%d.%d\n", (uint8_t)(ipv4->srcIP), (uint8_t)(ipv4->srcIP >> 8),
               (uint8_t)(ipv4->srcIP >> 16), (uint8_t)(ipv4->srcIP >> 24));
        dhcp_ip = swap32(ipv4->srcIP);
        printk("IP: %d.%d.%d.%d\n", nip1, nip2, nip3, nip4);
        ip = swap32(ip);

        unsigned char *options = &dhcp->bp_options[0];
        alloc_frame_line(get_page((uint32_t)options, 1, get_current_directory()), (uint32_t)options,
                         1, 0);
        while (options[0] != 0xff) {
            if (options[0] == MESSAGE_TYPE_DNS) {
                logkf("DNS: %d.%d.%d.%d\n", options[2], options[3], options[4], options[5]);
                dns = swap32(*(uint32_t *)&options[2]);
            } else if (options[0] == MESSAGE_TYPE_REQ_SUBNET_MASK) {
                logkf("Subnet Mask: %d.%d.%d.%d\n", options[2], options[3], options[4], options[5]);
                submask = swap32(*(uint32_t *)&options[2]);
            } else if (options[0] == MESSAGE_TYPE_ROUTER) {
                // waitif(true);
                logkf("Gateway: %d.%d.%d.%d\n", options[2], options[3], options[4], options[5]);

                gateway = swap32(*(uint32_t *)&options[2]);
            }
            options += options[1] + 2;
        }
    }
}
