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
        logkf((char *)("Cannot find IIC Master Controller.\n"));
        return;
    } else {
        logkf((char *)("Find IIC Master Controller.\n"));
        IIC_Master  *iic_master     = NULL;
        unsigned int address        = Get_iic_masterAddress(IIC_masterController);
        iic_master->Control         = address;
        IIC_slaveNode iic_slaveList = iic_slaveAlloc(NULL);

        //Initiative PCA9685
        IIC_Slave     pca9685     = init_pca9685(0x40);
        IIC_slaveNode pca9685Node = iic_slaveAlloc(&pca9685);
        iic_slaveAppend(&iic_slaveList.next, &pca9685Node);
        UNUSED(pca9685);
    }
}
