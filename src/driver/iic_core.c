#pragma once

#include "iic_core.h"

void iic_init(void) {
    pci_device_t *IIC_Master_Controller = pci_find_class(0x0C800000);
    if (IIC_Master_Controller == NULL) {
        klogf(false, "Cannot find IIC Master Controller.\n");
        return;
    } else {
        klogf(true, "Find IIC Master Controller.\n");
    }
}

bool crc_check(IIC_Data *frame) {

    uint32_t *data = frame->data;
    uint8_t crc = crc8(data, frame->data_len);
    if (crc != frame->crc) {
        printk("CRC error!\n");
        return false;
    }else{
        return true;
    }
}

uint32_t iic_data_transfer(IIC_Data *frame) {

    if (crc_check(frame)) {
        // 将原始数据转换为32位值
        uint32_t original_data = 0;
        for (uint8_t i = 0; i < frame->data_len; i++) {
            original_data |= (uint32_t)
            data[i] << (8 * i);
        }
        return original_data;
    }
}
void iic_start(pci_device_t *IIC_Device) {
    // IIC主机控制器基地址发送起始信号
    base_address_register iic_bar = find_bar(IIC_Device, 0);
    uint32_t base_address = (uint32_t)(iic_bar.address);
    io_out8(base_address + 0x10, 0x01);
}
void iic_stop(pci_device_t *IIC_Device) {
    // IIC主机控制器基地址发送停止信号
    base_address_register iic_bar = find_bar(IIC_Device, 0);
    uint32_t base_address = (uint32_t)(iic_bar.address);
    io_out8(base_address + 0x14, 0x00);
}


