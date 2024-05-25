#include "../include/net.h"
#include "../include/vdisk.h"
#include "../include/etherframe.h"

network_card network_card_CTL[25];
static uint8_t* IP_Packet_Base[16] = {NULL, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL};

void netcard_send(unsigned char* buffer, unsigned int size) {
    for (int i = 0; i < 25; i++) {
        if (network_card_CTL[i].use) {
            if (DriveSemaphoreTake(GetDriveCode("NETCARD_DRIVE"))) {
                // printk("Send....%s %d
                // %d\n",network_card_CTL[i].card_name,network_card_CTL[i].use,i);
                network_card_CTL[i].Send(buffer, size);
                DriveSemaphoreGive(GetDriveCode("NETCARD_DRIVE"));
                break;
            }
        }
    }
}

