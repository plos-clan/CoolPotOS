#include "stdint.h"
#include "crc.h"

// CRC-8
unsigned char crc8(unsigned int *data, unsigned char len) {
    unsigned char crc = 0;

    for (unsigned char i = 0; i < len; i++) {
        crc ^= data[i];
        for (unsigned char j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
