#ifndef COOLPOTOS_IIC_H
#define COOLPOTOS_IIC_H

#include "stdbool.h"
#include "list.h"
#include "io.h"
#include "crc.h"

typedef struct {
    uint32_t address;          // 地址
    uint8_t reg_address;       // 寄存器首地址
    uint32_t freq;             // 频率
    void (*init)(void);        // 设备初始化函数
} IIC_Device;
typedef struct {
    uint8_t rw_flag;           // 读写标志（0x00：写，0x01：读）
    uint8_t start;             // 数据段始标（0x01）
    uint32_t address;          // IIC设备地址
    uint8_t reg_address;       // IIC寄存器地址
    uint8_t data_len;          // 数据长度（字节计）
    uint32_t *data;            // 数据指针
    uint8_t crc;               // CRC校验值
    uint8_t stop;              // 数据段末标（0x01）
} IIC_Data;

void crc_check(IIC_Data *frame);

#endif //COOLPOTOS_IIC_H
