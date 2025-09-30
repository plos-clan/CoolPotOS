#include "iic/iic_core.h"

/**
 * @brief IIC Master Controller Initialization
 *
 * This function initializes the IIC Master Controller.
 * It first finds the IIC Master Controller from the PCI device list.
 * If the IIC Master Controller is found, it initializes it and allocates a linked list for slaves.
 * @return void
 */
void init_iic(void) {
    pci_device_t *IIC_masterController = (pci_device_t *)(pci_find_class(0x0C800000));
    if (IIC_masterController == NULL) {
        logkf((char *)("iic: Cannot find IIC Master Controller.\n"));
        return;
    } else {
        logkf((char *)("iic: Find IIC Master Controller.\n"));
        IIC_Master *iic_master      = 0;
        uint32_t    address         = Get_iic_masterAddress(IIC_masterController);
        iic_master->Control         = address;
        IIC_slaveNode iic_slaveList = iic_slaveAlloc(NULL);

        // Initiative PCA9685
        IIC_Slave     pca9685     = init_pca9685(0x40);
        IIC_slaveNode pca9685Node = iic_slaveAlloc(&pca9685);
        iic_slaveAppend(&iic_slaveList.slave_node, &pca9685Node);
    }
}
