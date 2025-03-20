#include "socket.h"
#include "udp.h"
#include "tcp.h"

static struct Socket sockets[MAX_SOCKET_NUM];

static void socket_udp_send(struct Socket *socket, uint8_t *data,
                            uint32_t size) {
    udp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                      socket->localPort, data, size);
}

static void socket_tcp_send(struct Socket *socket, uint8_t *data,
                            uint32_t size) {
    while(socket->state != SOCKET_TCP_ESTABLISHED);
    uint32_t s = size;
    tcp_provider_send(socket->remoteIP, socket->localIP, socket->remotePort,
                      socket->localPort, socket->seqNum, socket->ackNum, 0, 1, 0,
                      0, 0, 0, 0, 0, data, s);
    socket->seqNum += s;
    //io_delay();
}

void socket_init() {
    for (int i = 0; i < MAX_SOCKET_NUM; i++) {
        sockets->state = SOCKET_FREE;
    }
}

struct Socket *Socket_Find(uint32_t dstIP, uint16_t dstPort, uint32_t srcIP,
                           uint16_t srcPort, uint8_t protocol) {
    for (int i = 0; i != MAX_SOCKET_NUM; i++) {
        if (srcIP == sockets[i].localIP && dstIP == sockets[i].remoteIP &&
            srcPort == sockets[i].localPort && dstPort == sockets[i].remotePort &&
            protocol == sockets[i].protocol && sockets[i].state != SOCKET_FREE) {
            return (struct Socket *)&sockets[i];
        } else if (srcIP == sockets[i].localIP && srcPort == sockets[i].localPort &&
                   protocol == sockets[i].protocol &&
                   sockets[i].state == SOCKET_TCP_LISTEN) {
            return (struct Socket *)&sockets[i];
        }
    }
    return (struct Socket *) -1;
}