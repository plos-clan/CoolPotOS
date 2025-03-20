#pragma once

#include "crc.h"
#include "ctypes.h"
#include "io.h"
#include "klog.h"
#include "pci.h"

//eanble list implementation
#define LIST_IMPLEMENTATION 0x01
#include "list.h"

#define IIC_Slave_Flag_Busy  0x01
#define IIC_Slave_Flag_Empty 0x00
#define IIC_Slave_Flag_START 0x01
#define IIC_Slave_Flag_STOP  0x00
#define IIC_Slave_Flag_ACK   0x01
#define IIC_Slave_Flag_NoAck 0x00
#define IIC_Slave_Flag_Addr  0x01
#define IIC_Slave_Flag_Data  0x01

#define IIC_Master_Write 0x00
#define IIC_Master_Read  0x01

#define Freq_100 0x64
#define Freq_400 0x190

typedef struct IIC_Master {
    uint8_t Control;   // 控制寄存器
    uint8_t Status;    // 状态寄存器
    uint8_t Data;      // 数据寄存器
    uint8_t Address;   // 地址寄存器
    uint8_t Clock;     // 时钟寄存器
    uint8_t Transfer;  // 传输寄存器
    uint8_t Interrupt; // 中断寄存器
    uint8_t Error;     // 错误寄存器
} IIC_Master;

typedef struct IIC_Slave {
    unsigned int  address;     // 地址
    unsigned int  reg_address; // 寄存器首地址
    unsigned int  freq;        // 频率
    unsigned char flag;        // 状态
    void (*init)(void);        // 设备初始化
    void (*probe)(void);       // 挂载设备
    void (*remove)(void);      // 卸载设备
} IIC_Slave;

typedef struct IIC_Slave_Node {
    IIC_Slave slave;
    list_t    next;
} IIC_Slave_Node;

typedef struct IIC_Data {
    unsigned char start;    // 数据段始标（0x01）
    unsigned char data_len; // 数据长度（字节计）
    unsigned int *data;     // 数据指针
    unsigned char crc;      // CRC校验值
    unsigned char stop;     // 数据段末标（0x01）
} IIC_Data;

void           iic_init(void);
bool           crc_check(IIC_Data *);
IIC_Slave_Node iic_slave_alloc(IIC_Slave *);
void           iic_slave_append(list_t *, IIC_Slave_Node *);
void           iic_slave_delete(list_t *head, IIC_Slave_Node *node);
void           iic_slave_foreach(list_t *head, void (*func)(IIC_Slave *));
unsigned int   iic_data_transfer(IIC_Data *);
unsigned int   Get_iic_master_address(pci_device_t *);
void           iic_start(IIC_Master *);
void           iic_stop(IIC_Master *);
void           iic_send_start(IIC_Master *);
void           iic_send_stop(IIC_Master *);
void           iic_send_byte(IIC_Master *, unsigned char);
char           iic_receive_byte(IIC_Master *);
