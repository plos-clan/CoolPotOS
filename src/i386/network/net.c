#include "net.h"
#include "pcnet.h"
#include "socket.h"
#include "klog.h"

uint8_t mac0, mac1, mac2, mac3, mac4, mac5;

network_card network_card_CTL[25];
static uint8_t *IP_Packet_Base[16] = {NULL, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL};

void net_setup(){
    socket_init();
    pcnet_setup();
    klogf(true,"NetworkManager initialize.");
}

void netcard_send(unsigned char *buffer, unsigned int size) {
    for (int i = 0; i < 25; i++) {
        if (network_card_CTL[i].use) {
            // printk("Send....%s %d
            // %d\n",network_card_CTL[i].card_name,network_card_CTL[i].use,i);
            network_card_CTL[i].Send(buffer, size);
            break;
        }
    }
}