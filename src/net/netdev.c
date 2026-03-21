#include "errno.h"
#include "krlibc.h"
#include "mem/alloc.h"
#include "net/netdev.h"

static netdev_t *netdevs[MAX_NETDEV_NUM] = {NULL};

void regist_netdev(void *desc, uint8_t *mac, uint32_t mtu, netdev_send_t send,
                   netdev_recv_t recv) {
    for (int i = 0; i < MAX_NETDEV_NUM; i++) {
        if (netdevs[i] == NULL) {
            netdevs[i] = malloc(sizeof(netdev_t));
            if (netdevs[i] == NULL) {
                return;
            }
            netdevs[i]->desc = desc;
            netdevs[i]->mtu = mtu;
            memcpy(netdevs[i]->mac, mac, sizeof(netdevs[i]->mac));
            netdevs[i]->send = send;
            netdevs[i]->recv = recv;
            break;
        }
    }
}

netdev_t *get_default_netdev(void) { return netdevs[0]; }

int netdev_send(netdev_t *dev, void *data, uint32_t len) {
    if (dev == NULL || data == NULL) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }
    return dev->send(dev->desc, data, len);
}

int netdev_recv(netdev_t *dev, void *data, uint32_t len) {
    if (dev == NULL || data == NULL) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }
    return dev->recv(dev->desc, data, len);
}
