#include "dns.h"
#include "krlibc.h"
#include "ipv4.h"
#include "udp.h"
#include "etherframe.h"

uint32_t dns_parse_ip_result = 0;

void dns_handler(void *base) {
    struct DNS_Header *dns_header =
            (struct DNS_Header *)(base + sizeof(struct EthernetFrame_head) +
                                  sizeof(struct IPV4Message) +
                                  sizeof(struct UDPMessage));
    if (swap16(dns_header->ID) == DNS_Header_ID) {
        uint8_t *p = (uint8_t *)(dns_header) + sizeof(struct DNS_Header);
        p += strlen(p) + sizeof(struct DNS_Question) - 1;
        struct DNS_Answer *dns_answer = (struct DNS_Answer *)p;
        dns_parse_ip_result = swap32(*(uint32_t *)&dns_answer->RData[0]);
    }
}
