#include "../include/ahci.h"
#include "../include/pci.h"
#include "../include/printf.h"
#include "../include/vdisk.h"

void ahci_init(){
    pci_device_t *ahci_ctrl = pci_find_class(0x0106);
    if(ahci_ctrl == NULL){
        klogf(false,"Cannot find ahci device\n");
        return;
    } else klogf(true,"Find AHCI Controller\n");

    uint32_t bar5 = pci_dev_read32(ahci_ctrl, PCI_BASE_ADDR5) & 0xFFFFFFF0;

}