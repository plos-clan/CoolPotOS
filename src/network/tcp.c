#include "../include/tcp.h"
#include "../include/memory.h"
#include "../include/ipv4.h"
#include "../include/dhcp.h"
#include "../include/socket.h"
#include "../include/printf.h"
#include "../include/etherframe.h"

static uint16_t ident = 0;

void tcp_provider_send(uint32_t dstIP, uint32_t srcIP, uint16_t dstPort,
                       uint16_t srcPort, uint32_t Sequence, uint32_t ackNum,
                       bool URG, bool ACK, bool PSH, bool RST, bool SYN, bool FIN,
                       bool ECE, bool CWR, uint8_t *data, uint32_t size) {
    uint32_t s =
            SYN ? (sizeof(struct TCPPesudoHeader) + sizeof(struct TCPMessage) + size +
                   4)
                : (sizeof(struct TCPPesudoHeader) + sizeof(struct TCPMessage) + size);
    uint8_t *dat = (uint8_t *)kmalloc(s);
    struct TCPPesudoHeader *phdr = (struct TCPPesudoHeader *)dat;
    struct TCPMessage *tcp =
            (struct TCPMessage *)(dat + sizeof(struct TCPPesudoHeader));
    memcpy((void *)(tcp) + (SYN ? (sizeof(struct TCPMessage) + 4)
                                : sizeof(struct TCPMessage)),
           (void *)data, size);
    phdr->dstIP = swap32(dstIP);
    phdr->srcIP = swap32(srcIP);
    phdr->protocol = 0x0600;
    phdr->totalLength = swap16(sizeof(struct TCPMessage) + size);
    tcp->dstPort = swap16(dstPort);
    tcp->srcPort = swap16(srcPort);
    tcp->seqNum = swap32(Sequence);
    tcp->ackNum = swap32(ackNum);
    tcp->headerLength = sizeof(struct TCPMessage) / 4;
    tcp->reserved = 0;
    tcp->URG = URG;
    tcp->ACK = ACK;
    tcp->PSH = PSH;
    tcp->RST = RST;
    tcp->SYN = SYN;
    tcp->FIN = FIN;
    tcp->ECE = ECE;
    tcp->CWR = CWR;
    tcp->window = 0xffff;
    tcp->pointer = 0;
    if (SYN) {
        tcp->options[0] = 0xb4050402;
        phdr->totalLength = swap16(swap16(phdr->totalLength) + 4);
        tcp->headerLength += 1;
    }
    tcp->checkSum = 0;
    tcp->checkSum = CheckSum((uint16_t *)dat, s);
    IPV4ProviderSend(6, IPParseMAC(dstIP), dstIP, srcIP, (uint8_t *)tcp,
                     s - sizeof(struct TCPPesudoHeader));
    kfree((void *)dat);
    return;
}

void tcp_handler(void *base) {
    struct IPV4Message *ipv4 =
            (struct IPV4Message *)(base + sizeof(struct EthernetFrame_head));
    struct TCPMessage *tcp =
            (struct TCPMessage *)(base + sizeof(struct EthernetFrame_head) +
                                  sizeof(struct IPV4Message));
    struct Socket *socket =
            Socket_Find(swap32(ipv4->srcIP), swap16(tcp->srcPort),
                        swap32(ipv4->dstIP), swap16(tcp->dstPort), TCP_PROTOCOL);
    if (socket == -1) {
        // printk("Not found %08x %d %08x %d\n",swap32(ipv4->srcIP),
        // swap16(tcp->srcPort), swap32(ipv4->dstIP), swap16(tcp->dstPort));
        return;
    }
    uint8_t flags = (tcp->ACK << 4) | (tcp->PSH << 3) | (tcp->SYN << 1) |
                    tcp->FIN; // 只看ACK,PSH,SYN,FIN四个flags
    if (tcp->RST) {
        socket->state = SOCKET_TCP_CLOSED;
    }
    if (socket->state != SOCKET_TCP_CLOSED) {
        switch (flags) {
            case 0x12: // 00010010  ACK | SYN
                if (socket->state == SOCKET_TCP_SYN_SENT) {
                    socket->state = SOCKET_TCP_ESTABLISHED;
                    socket->ackNum = swap32(tcp->seqNum) + 1;
                    if ((uint16_t)tcp->options == 0x0402) {
                        uint16_t MSS_ = swap32(tcp->options[0]) & 0xffff;
                        socket->MSS = (MSS_Default >= MSS_) ? MSS_ : MSS_Default;
                    }
                    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                      0, 0, 0, 0, 0, 0, 0, 0);
                }
                break;
            case 0x02: // 00000010 SYN
                if (socket->state == SOCKET_TCP_LISTEN) {
                    socket->state = SOCKET_TCP_SYN_RECEIVED;
                    socket->remoteIP = swap32(ipv4->srcIP);
                    socket->remotePort = swap16(tcp->srcPort);
                    socket->ackNum = swap32(tcp->seqNum) + 1;
                    socket->seqNum = 0;
                    if ((uint16_t)tcp->options == 0x0402) {
                        uint16_t MSS_ = swap32(tcp->options[0]) & 0xffff;
                        socket->MSS = (MSS_Default >= MSS_) ? MSS_ : MSS_Default;
                    }
                    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                      0, 0, 1, 0, 0, 0, 0, 0);
                    socket->seqNum++;
                }
                break;
            case 0x10: // 00010000 ACK
                if (socket->state == SOCKET_TCP_SYN_RECEIVED) {
                    socket->state = SOCKET_TCP_ESTABLISHED;
                } else if (socket->state == SOCKET_TCP_FIN_WAIT1) {
                    socket->state = SOCKET_TCP_FIN_WAIT2;
                } else if (socket->state == SOCKET_TCP_CLOSE_WAIT) {
                    socket->state = SOCKET_TCP_CLOSED;
                }
                if (tcp->ACK && !tcp->CWR && !tcp->ECE && !tcp->PSH &&
                    !tcp->URG) { // Only ACK=1
                    goto _default;
                }
            case 0x01: // 00000001 FIN
            case 0x11: // 00010001 ACK | FIN
                if (socket->state == SOCKET_TCP_ESTABLISHED) {
                    socket->state = SOCKET_TCP_CLOSE_WAIT;
                    socket->ackNum++;
                    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                      0, 0, 0, 0, 0, 0, 0, 0);
                    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                      0, 0, 0, 1, 0, 0, 0, 0);
                } else if (socket->state == SOCKET_TCP_FIN_WAIT1 ||
                           socket->state == SOCKET_TCP_FIN_WAIT2) {
                    socket->state = SOCKET_TCP_CLOSED;
                    socket->ackNum++;
                    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                      0, 0, 0, 0, 0, 0, 0, 0);
                } else if (socket->state == SOCKET_TCP_CLOSE_WAIT) {
                    socket->state = SOCKET_TCP_CLOSED;
                }
                break;
            default:
            _default:
                // TCP 传输
                if ((swap16(ipv4->totalLength) - sizeof(struct IPV4Message) -
                     (tcp->headerLength * 4)) == socket->MSS) {
                    printf("TCP Segment.\n");
                    break;
                }
                if (socket->ackNum == swap32(tcp->seqNum) &&
                    swap16(ipv4->totalLength) !=
                    (sizeof(struct IPV4Message) + (tcp->headerLength * 4))) {
                    if (socket->Handler != NULL) {
                        socket->Handler(socket, base);
                    }
                    socket->ackNum += swap16(ipv4->totalLength) -
                                      sizeof(struct IPV4Message) - (tcp->headerLength * 4);
                }
                tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                                  socket->localPort, socket->seqNum, socket->ackNum, 0, 1,
                                  1, 0, 0, 0, 0, 0, 0, 0);
                break;
        }
    }
}