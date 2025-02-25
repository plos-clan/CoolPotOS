#include "xhci.h"
#include "pcie.h"
#include "kprint.h"

void xhci_setup(){
    pcie_device_t *device = pcie_find_class(0xc0300);
    if(device == NULL) return;

    uint32_t command = pcie_read_command(device,2);
    command &= ~(PCI_COMMAND_IO | PCI_COMMAND_INT_DISABLE);
    command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
    pcie_write_command(device,2,command);

    kinfo("Loading USB 3.0 driver.");
}
