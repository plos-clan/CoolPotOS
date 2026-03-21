#pragma once

#include "types.h"

typedef int (*netdev_send_t)(void *dev, void *data, uint32_t len);
typedef int (*netdev_recv_t)(void *dev, void *data, uint32_t len);

#define MAX_NETDEV_NUM           8
#define NETDEV_ETH_FRAME_OVERHEAD 18

static inline uint32_t netdev_max_frame_len(uint32_t mtu) {
    return mtu + NETDEV_ETH_FRAME_OVERHEAD;
}

typedef struct netdev {
    uint8_t mac[6];
    uint32_t mtu;
    void *desc;
    netdev_send_t send;
    netdev_recv_t recv;
} netdev_t;

void regist_netdev(void *desc, uint8_t *mac, uint32_t mtu, netdev_send_t send,
                   netdev_recv_t recv);

netdev_t *get_default_netdev(void);

int netdev_send(netdev_t *dev, void *data, uint32_t len);
int netdev_recv(netdev_t *dev, void *data, uint32_t len);
