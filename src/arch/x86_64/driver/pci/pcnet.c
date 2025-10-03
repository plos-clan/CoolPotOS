#include "pcnet.h"
#include "io.h"
#include "kprint.h"
#include "pci.h"

void pcnet_setup() {
    pci_device_t *device = pci_find_class(0x020000);
    if (device == NULL) { return; }
    uint32_t conf  = pci_read_command(device, 0x04);
    conf          |= PCI_COMMAND_MEMORY;
    conf          |= PCI_RCMD_BUS_MASTER;
    conf          |= PCI_COMMAND_IO;
    pci_write_command(device, 0x04, conf);

    // pci_set_msi(device, pcnet);

    kinfo("Setup pcnet driver.");
}
