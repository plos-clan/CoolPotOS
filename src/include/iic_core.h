#pragma once

#include "ctypes.h"

#include "list.h"
#include "io.h"
#include "crc.h"
#include "klog.h"
#include "pci.h"

typedef struct {
    unsigned int address;          // 地址
    unsigned int reg_address;       // 寄存器首地址
    unsigned int freq;             // 频率
    unsigned char flag;             // 状态
    void (*init)(void);        // 设备初始化
    void (*probe)(void);       // 挂载设备
    void (*remove)(void);     // 卸载设备
} IIC_Device;
typedef struct {
    unsigned char rw_flag;           // 读写标志（0x00：写，0x01：读）
    unsigned char start;             // 数据段始标（0x01）
    unsigned char data_len;          // 数据长度（字节计）
    unsigned int *data;            // 数据指针
    unsigned char crc;               // CRC校验值
    unsigned char stop;              // 数据段末标（0x01）
} IIC_Data;

void iic_init(void);
bool crc_check(IIC_Data *frame);
unsigned int iic_data_transfer(IIC_Data *frame);
void iic_start(pci_device_t *IIC_Device);
void iic_stop(pci_device_t *IIC_Device);
