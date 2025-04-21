#include "rtl8169.h"
#include "hhdm.h"
#include "io.h"
#include "kprint.h"
#include "pci.h"

RTL8169_Regs *rtl8169;
RTL8169_Desc  tx_desc[TX_DESC_COUNT] __attribute__((aligned(256)));
RTL8169_Desc  rx_desc[RX_DESC_COUNT] __attribute__((aligned(256)));

void rtl8169_setup() {
    pci_device_t *device = pci_find_class(0x020000);
    if (device == NULL) { return; }

    uint32_t conf  = pci_read_command(device, 0x04);
    conf          |= PCI_RCMD_BUS_MASTER;
    conf          |= PCI_COMMAND_MEMORY;
    conf          |= PCI_COMMAND_IO;
    pci_write_command(device, 0x04, conf);
    rtl8169 = (RTL8169_Regs *)device->bars[0].address;
    if (rtl8169 == NULL) {
        kerror("Rtl8169 pci bar 0 is null.");
        return;
    }

    rtl8169 = phys_to_virt((uint64_t)rtl8169);

    volatile uint32_t *rtl8169_mmio = (volatile uint32_t *)(((uint64_t)rtl8169) & ~0xF);

    rtl8169_mmio[0x37 / 4] |= (1 << 4);
    while (rtl8169_mmio[0x37 / 4] & (1 << 4)) {
        __asm__("pause");
    }
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = (rtl8169_mmio[0x00 + i] & 0xFF);
    }

    kinfo("Rtl8169 mac: %x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
