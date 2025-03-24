#include "pcnet.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "pci.h"
#include "pcie.h"

void pcnet_setup() {
    pcie_device_t *device = pcie_find_class(0x020000);
    if (device == NULL) {
        pci_device_t pci_device = pci_find_class(0x020000);
        if (pci_device == NULL) return;
        uint32_t conf  = pci_read_command_status(pci_device);
        conf          |= PCI_COMMAND_MEMORY;
        conf          |= PCI_RCMD_BUS_MASTER;
        conf          |= PCI_COMMAND_IO;
        pci_write_command_status(pci_device, conf);
        pci_set_msi(pci_device, pcnet);
    } else {
        uint32_t conf = pcie_read_command(device, 0x04);
        conf         |= PCI_COMMAND_MEMORY;
        conf         |= PCI_RCMD_BUS_MASTER;
        conf         |= PCI_COMMAND_IO;
        pcie_write_command(device, 0x04, conf);
        pcie_set_msi(device, pcnet);
    }

    kinfo("Setup pcnet driver.");
}
