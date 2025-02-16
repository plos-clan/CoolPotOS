#include "iic/iic_core.h"

void init_iic(void) {
    pci_device_t *IIC_Master_Controller = (pci_device_t *)(pci_find_class(0x0C800000));
    if (IIC_Master_Controller == NULL) {
        logkf((char *)("Cannot find IIC Master Controller.\n"));
        return;
    } else {
        logkf((char *)("Find IIC Master Controller.\n"));
        IIC_Master *iic_master = NULL;
        unsigned int address = Get_iic_master_address(IIC_Master_Controller);
        iic_master->Control = address;
        IIC_Slave_Node iic_slave_list = iic_slave_alloc(NULL);
    }
}