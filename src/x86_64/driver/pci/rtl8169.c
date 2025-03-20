#include "rtl8169.h"
#include "hhdm.h"
#include "io.h"
#include "kprint.h"
#include "pci.h"
#include "pcie.h"

RTL8169_Regs *rtl8169;
RTL8169_Desc  tx_desc[TX_DESC_COUNT] __attribute__((aligned(256)));
RTL8169_Desc  rx_desc[RX_DESC_COUNT] __attribute__((aligned(256)));

void rtl8169_reset() {
    rtl8169->Command = 0x10;
    while (rtl8169->Command & 0x10)
        __asm__ volatile("pause");
}

void rtl8169_setup_descriptors() {
    rtl8169->TxDesc = (uint32_t)virt_to_phys((uint64_t)&tx_desc); // 设置 TX 描述符基地址
    rtl8169->RxDesc = (uint32_t)&rx_desc;                         // 设置 RX 描述符基地址
}

void rtl8169_setup() {
    pcie_device_t *device = pcie_find_class(0x020000);
    if (device == NULL) {
        pci_device_t pci_device = pci_find_class(0x020000);
        if (pci_device == NULL) return;
        uint32_t conf  = pci_read_command_status(pci_device);
        conf          |= PCI_COMMAND_MEMORY;
        conf          |= PCI_RCMD_BUS_MASTER;
        conf          |= PCI_COMMAND_IO;
        pci_write_command_status(pci_device, conf);
        base_address_register reg = find_bar(pci_device, 0);
        if (reg.address == NULL) {
            kerror("Rtl8169 pci bar 0 is null.");
            return;
        }
        rtl8169 = (RTL8169_Regs *)phys_to_virt((uint64_t)reg.address);
    } else {
        uint32_t conf  = pcie_read_command(device, 0x04);
        conf          |= PCI_RCMD_BUS_MASTER;
        conf          |= PCI_COMMAND_MEMORY;
        conf          |= PCI_COMMAND_IO;
        pcie_write_command(device, 0x04, conf);
        rtl8169 = (RTL8169_Regs *)device->bars[0].address;
        if (rtl8169 == NULL) {
            kerror("Rtl8169 pci bar 0 is null.");
            return;
        }
        rtl8169 = phys_to_virt((uint64_t)rtl8169);
    }

    rtl8169->Command = 0x0C;
    rtl8169_setup_descriptors();

    kinfo("Rtl8169 mac: %x:%x:%x:%x:%x:%x", rtl8169->MAC[0] & 0xFF, (rtl8169->MAC[0] >> 8) & 0xFF,
          (rtl8169->MAC[0] >> 16) & 0xFF, (rtl8169->MAC[0] >> 24) & 0xFF, rtl8169->MAC[1] & 0xFF,
          (rtl8169->MAC[1] >> 8) & 0xFF);
}
