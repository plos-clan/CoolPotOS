#include "pcnet.h"
#include "acpi.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "pci.h"
#include "pcie.h"

void pcnet_setup() {
    pcie_device_t *device = pcie_find_class(0x10220000);
    if (device == NULL) {
        pci_device_t pci_device = pci_find_class(0x10220000);
        if (pci_device == NULL) return;
        uint32_t conf  = pci_read_command_status(pci_device);
        conf          |= PCI_COMMAND_MEMORY;
        conf          |= PCI_RCMD_BUS_MASTER;
        conf          |= PCI_COMMAND_IO;
        pci_write_command_status(pci_device, conf);
        base_address_register reg = find_bar(pci_device, 0);
        if (reg.address == NULL) {
            kerror("Pcnet pci bar 0 is null.");
            return;
        }
        ioapic_add(pcnet, pci_get_drive_irq(pci_device->bus, pci_device->slot, pci_device->func));
    } else {
        ioapic_add(pcnet, pci_get_drive_irq(device->bus, device->slot, device->func));
    }
}
