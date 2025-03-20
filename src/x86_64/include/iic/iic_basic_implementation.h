#pragma once

#include <klog.h>

#ifdef ALL_IMPLEMENTATION
#    define IIC_BASIC_IMPLEMENTATION
#endif

#include "crc.h"
#include "io.h"
#include "list.h"
#include "pci.h"

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

/**
 * @struct IIC_Master
 * @brief IIC主机控制器
 */
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

/**
 * @struct IIC_Slave
 * @brief IIC从机
 */
typedef struct IIC_Slave {
    uint32_t address;     // 地址
    uint32_t reg_address; // 寄存器首地址
    uint32_t freq;        // 频率
    uint8_t  flag;        // 状态
    void (*probe)(void);  // 挂载设备
    void (*remove)(void); // 卸载设备
} IIC_Slave;

/**
 * @struct IIC_slaveNode
 * @brief IIC从机节点
 */
typedef struct IIC_slaveNode {
    IIC_Slave slave;
    list_t    next;
} IIC_slaveNode;

/**
 * @struct IIC_Data
 * @brief IIC数据
 */
typedef struct IIC_Data {
    uint16_t  start;    // 数据段始标（0x01）
    uint16_t  data_len; // 数据长度（字节计）
    uint32_t *data;     // 数据指针
    uint16_t  crc;      // CRC校验值
    uint16_t  stop;     // 数据段末标（0x01）
} IIC_Data;

#ifdef IIC_BASIC_IMPLEMENTATION

bool          crc_check(IIC_Data *);
IIC_slaveNode iic_slaveAlloc(IIC_Slave *);
void          iic_slaveAppend(list_t *, IIC_slaveNode *);
void          iic_slaveDelete(list_t *, IIC_slaveNode *);
void          iic_slaveForeach(list_t *, void (*)(IIC_Slave *));
uint32_t      iic_dataTransfer(IIC_Data *);
uint8_t       Get_iic_masterAddress(pci_device_t *);
void          iic_start(IIC_Master *);
void          iic_stop(IIC_Master *);
void          iic_sendStart(IIC_Master *);
void          iic_sendStop(IIC_Master *);
void          iic_sendByte(IIC_Master *, uint8_t);
uint8_t       iic_receiveByte(IIC_Master *);

#endif
