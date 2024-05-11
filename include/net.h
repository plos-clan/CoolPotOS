#ifndef CRASHPOWEROS_NET_H
#define CRASHPOWEROS_NET_H

#include "../include/common.h"

typedef struct {
    bool (*find)();
    void (*init)();
    void (*Send)(unsigned char* buffer, unsigned int size);
    char card_name[50];
    int use;  // 正在使用
    int flag;
} network_card;

void netcard_send(unsigned char* buffer, unsigned int size);

#endif
