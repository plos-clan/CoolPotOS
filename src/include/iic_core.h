#pragma once

#include "ctypes.h"

#include "list.h"
#include "io.h"
#include "crc.h"
#include "klog.h"
#include "pci.h"

#define IIC_Salve_Flag_Busy  0x00
#define IIC_Salve_Flag_Empty 0x01
#define IIC_Salve_Flag_Error 0x02

#define IIC_Master_Write 0x00
#define IIC_Master_Read  0x01

#define IIC_Slave_NoAck  0x00
#define IIC_Slave_Ack    0x01

typedef struct IIC_Master {
    unsigned int address;           // 地址
    unsigned int reg_address;       // 寄存器首地址
    unsigned int freq;              // 频率
} IIC_Master;

typedef struct IIC_Slave {
    unsigned int address;           // 地址
    unsigned int reg_address;       // 寄存器首地址
    unsigned int freq;              // 频率
    unsigned char flag;             // 状态
    void (*init)(void);             // 设备初始化
    void (*probe)(void);            // 挂载设备
    void (*remove)(void);           // 卸载设备
} IIC_Slave;

typedef struct IIC_Data {
    unsigned char start;            // 数据段始标（0x01）
    unsigned char data_len;         // 数据长度（字节计）
    unsigned int *data;             // 数据指针
    unsigned char crc;              // CRC校验值
    unsigned char stop;             // 数据段末标（0x01）
} IIC_Data;

void iic_init(void);
bool crc_check(IIC_Data *);
unsigned int iic_data_transfer(IIC_Data *);
unsigned int Get_iic_master_address(pci_device_t *);
void iic_Start(IIC_Master *);
void iic_Stop(IIC_Master *);
void iic_send_Start(IIC_Master *);
void iic_send_Stop(IIC_Master *);
void iic_send_Byte(IIC_Master *, unsigned char);
char iic_receive_Byte(IIC_Master *);
