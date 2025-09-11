#pragma once

#define MAX_NETDEV_NUM 8

#include "ctype.h"

typedef errno_t (*netdev_send_t)(void *dev, void *data, uint32_t len);
typedef errno_t (*netdev_recv_t)(void *dev, void *data, uint32_t len);

typedef struct netdev {
    uint8_t       mac[6];
    uint32_t      mtu;
    void         *desc;
    netdev_send_t send;
    netdev_recv_t recv;
} netdev_t;

void regist_netdev(void *desc, uint8_t *mac, uint32_t mtu, netdev_send_t send, netdev_recv_t recv);
errno_t netdev_send(netdev_t *dev, void *data, uint32_t len);
errno_t netdev_recv(netdev_t *dev, void *data, uint32_t len);
errno_t syscall_net_socket(int domain, int type, int protocol);
void    netfs_setup();
