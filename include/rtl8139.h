#ifndef CRASHPOWEROS_RTL8139_H
#define CRASHPOWEROS_RTL8139_H

#define CARD_VENDOR_ID 0x10EC
#define CARD_DEVICE_ID 0x8139
#define MAC0 0x00
#define MAC1 0x01
#define MAC2 0x02
#define MAC3 0x03
#define MAC4 0x04
#define MAC5 0x05
#define MAR 0x08
#define RBSTART 0x30
#define CAPR 0x38
#define CMD 0x37
#define IMR 0x3C
#define ISR 0x3E
#define CONFIG_1 0x52
#define TCR 0x40
#define RCR 0x44
#define TSAD0 0x20
#define TSD0 0x10

#include <stdint.h>

#endif
