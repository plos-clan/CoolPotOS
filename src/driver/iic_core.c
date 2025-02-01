#pragma once

#include "iic_core.h"

void iic_init(void) {
    pci_device_t *IIC_Master_Controller = pci_find_class(0x0C800000);
    if (IIC_Master_Controller == NULL) {
        klogf(false, "Cannot find IIC Master Controller.\n");
        return;
    } else {
        klogf(true, "Find IIC Master Controller.\n");
        IIC_Master *iiC_master;
        unsigned int Address = Get_iic_master_address(IIC_Master_Controller);
        iiC_master->address = Address;
        iiC_master->reg_address = Address;
    }
}


bool crc_check(IIC_Data *frame) {

    unsigned int *data = frame->data;
    char crc = crc8(data, frame->data_len);
    if (crc != frame->crc) {
        return false;
    }else{
        return true;
    }
}


unsigned int iic_data_transfer(IIC_Data *frame) {

    unsigned int *data = frame->data;
    if (crc_check(frame)) {
        // 将原始数据转换为32位值
        int original_data = 0;
        for (char i = 0; i < frame->data_len; i++) {
            original_data |= (int)data[i] << (8 * i);
        }
        return original_data;
    } else {
        return false;
    }
}

unsigned int Get_iic_master_address(pci_device_t *IIC_Master_Controller) {
    // 获取IIC主机控制器基地址
    base_address_register bar = find_bar(IIC_Master_Controller, 0);
    unsigned int base_address = (unsigned int)(bar.address);
    return base_address;
}

void iic_start(IIC_Master *IIC_Master) {
    // IIC主机控制器总线初始化
}

void iic_stop(IIC_Master *IIC_Master) {
    // 兼容性
}

void iic_send_Start(IIC_Master *IIC_Master) {
    // IIC主机控制器发送起始信号
}

void iic_send_Stop(IIC_Master *IIC_Master) {
    // IIC主机控制器发送停止信号
}

void iic_send_byte(IIC_Master *IIC_Master, unsigned char data) {
    // IIC主机控制器基地址发送一个字节
}

char iic_receive_byte(IIC_Master *IIC_Master) {
    // IIC主机控制器基地址接收一个字节
}
