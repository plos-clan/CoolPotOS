#include "e1000.h"
#include "kprint.h"
#include "pci.h"

void e1000_setup() {
    pci_device_t *device = pci_find_vid_did(0x8086, 0x100e);
    if (device == NULL) { return; }
    uint64_t e1000_mmio_base = device->bars[0].address;
    if (e1000_mmio_base == 0) { return; }
    kinfo("Setup e1000 driver at 0x%x.", e1000_mmio_base);
}
