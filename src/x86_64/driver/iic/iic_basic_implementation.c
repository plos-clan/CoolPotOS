//enable all implementation
#define ALL_IMPLEMENTATION

#include "iic/iic_basic_implementation.h"
#include "krlibc.h"

bool crc_check(IIC_Data *frame) {

    unsigned int *data = frame->data;
    char          crc  = crc8(data, frame->data_len);
    if (crc != frame->crc) {
        return false;
    } else {
        return true;
    }
}

IIC_slaveNode iic_slaveAlloc(IIC_Slave *slave) {
    IIC_slaveNode node;
    node.slave_node = list_alloc(slave);
    node.slave      = *slave;
    return node;
}

void iic_slaveAppend(list_t *head, IIC_slaveNode *node) {
    list_append(*head, node->slave_node);
}

void iic_slaveDelete(list_t *head, IIC_slaveNode *node) {
    list_delete(*head, node->slave_node);
}

void iic_slaveForeach(list_t *head, void (*func)(IIC_Slave *)) {
    list_foreach(*head, func);
}

uint32_t iic_dataTransfer(IIC_Data *frame) {

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

/**
 * @brief Get the base address of the IIC Master Controller.
 *
 * This function is used to get the base address of the IIC Master Controller.
 * It takes a pointer to a pci_device_t as an argument, which is the PCI device
 * structure of the IIC Master Controller.
 *
 * @param IIC_Master_Controller The pointer to the pci_device_t of the IIC Master Controller.
 *
 * @return The base address of the IIC Master Controller.
 */
uint8_t Get_iic_masterAddress(pci_device_t *IIC_Master_Controller) {

    base_address_register bar          = find_bar(*IIC_Master_Controller, 0);
    uint8_t               base_address = *bar.address;
    return base_address;
}

void iic_start(IIC_Master *IIC_Master) {
    // 启动兼容性
}

void iic_stop(IIC_Master *IIC_Master) {
    // 兼容性
}
