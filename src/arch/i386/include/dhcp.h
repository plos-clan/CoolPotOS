#pragma once

#define DHCP_CHADDR_LEN 16
#define DHCP_SNAME_LEN  64
#define DHCP_FILE_LEN   128

#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2

#define DHCP_HARDWARE_TYPE_10_EHTHERNET 1

#define MESSAGE_TYPE_PAD                0
#define MESSAGE_TYPE_REQ_SUBNET_MASK    1
#define MESSAGE_TYPE_ROUTER             3
#define MESSAGE_TYPE_DNS                6
#define MESSAGE_TYPE_DOMAIN_NAME        15
#define MESSAGE_TYPE_REQ_IP             50
#define MESSAGE_TYPE_DHCP               53
#define MESSAGE_TYPE_PARAMETER_REQ_LIST 55
#define MESSAGE_TYPE_END                255

#define DHCP_OPTION_DISCOVER 1
#define DHCP_OPTION_OFFER    2
#define DHCP_OPTION_REQUEST  3
#define DHCP_OPTION_PACK     4

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_MAGIC_COOKIE 0x63825363

#include "cptype.h"

struct DHCPMessage {
    uint8_t  opcode;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[DHCP_CHADDR_LEN];
    char     bp_sname[DHCP_SNAME_LEN];
    char     bp_file[DHCP_FILE_LEN];
    uint32_t magic_cookie;
    uint8_t  bp_options[0];
} __attribute__((packed));

int  dhcp_discovery(uint8_t *mac);
void dhcp_handler(void *base);