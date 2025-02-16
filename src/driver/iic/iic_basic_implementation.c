//enable all implementation
#define ALL_IMPLEMENTATION   0x01

#include "iic/iic_basic_implementation.h"
#include "krlibc.h"

bool crc_check(IIC_Data *frame) {

    unsigned int *data = frame->data;
    char crc = crc8(data, frame->data_len);
    if (crc != frame->crc) {
        return false;
    }else{
        return true;
    }
}


IIC_Slave_Node iic_slave_alloc(IIC_Slave *slave) {
    IIC_Slave_Node node;
    node.next = list_alloc(NULL);
    node.slave = *slave;
    return node;
}

void iic_slave_append(list_t *head, IIC_Slave_Node *node) {
    list_append(*head, node->next);
}

void iic_slave_delete(list_t *head, IIC_Slave_Node *node) {
    list_delete(*head, node->next);
}

void iic_slave_foreach(list_t *head, void (*func)(IIC_Slave *)) {
    UNUSED(func);
    list_foreach(*head, func);
}

uint32_t iic_data_transfer(IIC_Data *frame) {

    unsigned int *data = frame->data;
    if (crc_check(frame)) {
        // 将原始数据转换为32位值
        int original_data = 0;
        for (size_t i = 0; i < frame->data_len; i++) {
            original_data |= (int)data[i] << (8 * i);
        }
        return original_data;
    } else {
        return false;
    }
}

uint8_t Get_iic_master_address(pci_device_t *IIC_Master_Controller) {
    // 获取IIC主机控制器基地址
    base_address_register bar = find_bar(*IIC_Master_Controller, 0);
    uint8_t base_address = *bar.address;
    return base_address;
}

void iic_start(IIC_Master *IIC_Master) {
    UNUSED(IIC_Master);
    // 启动兼容性
}

void iic_stop(IIC_Master *IIC_Master) {
    UNUSED(IIC_Master);
    // 兼容性
}

void iic_send_start(IIC_Master *IIC_Master) {
    UNUSED(IIC_Master);
    // 起始信号兼容性
}

void iic_send_stop(IIC_Master *IIC_Master) {
    UNUSED(IIC_Master);
    // 停止信号兼容性
}

void iic_send_byte(IIC_Master *IIC_Master, unsigned char data) {
    UNUSED(IIC_Master);
    UNUSED(data);
    // 发送字节兼容性
}

uint16_t iic_receive_byte(IIC_Master *IIC_Master) {
    UNUSED(IIC_Master);
    // 接收字节兼容性
    return 0;
}
