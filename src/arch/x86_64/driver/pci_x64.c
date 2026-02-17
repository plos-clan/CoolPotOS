#include "driver/pci/pci.h"
#include "io.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "term/klog.h"

#define PCI_COMMAND_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

extern pci_device_t *pci_devices[PCI_DEVICE_MAX];
extern uint32_t pci_device_number;

uint32_t pci_read0(uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset) {
    uint32_t id = (1U << 31) | ((b & 0xff) << 16) | ((d & 0x1f) << 11) | ((f & 0x07) << 8)
                  | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    uint32_t result = io_in32(PCI_DATA_PORT);
    return result >> ((8 * (registeroffset & 2)) & 0xFF);
}

void pci_write0(
    uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset, uint32_t value
) {
    uint32_t id = (1U << 31) | ((b & 0xff) << 16) | ((d & 0x1f) << 11) | ((f & 0x07) << 8)
                  | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    io_out32(PCI_DATA_PORT, value);
}

pci_device_op_t pci_device_op = {
    .read  = pci_read0,
    .write = pci_write0,
};

void pci_scan_device_legacy(uint32_t bus, uint32_t equipment, uint32_t f) {
    pci_device_t *device = (pci_device_t *)malloc(sizeof(pci_device_t));
    memset(device, 0, sizeof(pci_device_t));
    device->op = &pci_device_op;

    uint32_t value_c    = device->op->read(bus, equipment, f, 0, PCI_CONF_REVISION);
    uint32_t class_code = value_c >> 8;

    uint16_t value_v   = device->op->read(bus, equipment, f, 0, PCI_CONF_VENDOR);
    uint16_t value_d   = device->op->read(bus, equipment, f, 0, PCI_CONF_DEVICE);
    uint16_t vendor_id = value_v & 0xffff;
    uint16_t device_id = value_d & 0xffff;

    uint32_t interrupt_value =
        device->op->read(device->bus, device->slot, device->func, device->segment, 0x3c);
    device->irq_line = interrupt_value & 0xff;
    device->irq_pin  = (interrupt_value >> 8) & 0xff;

    device->name       = pci_classname(class_code);
    device->vendor_id  = vendor_id;
    device->device_id  = device_id;
    device->class_code = class_code;
    device->segment    = 0;
    device->bus        = bus;
    device->slot       = equipment;
    device->func       = f;

    printk("Found PCI device: %#08lx name: %s\n", device->class_code, device->name);

    for (int i = 0; i < 6; i++) {
        int offset       = 0x10 + i * 4;
        uint32_t bar_low = device->op->read(bus, equipment, f, 0, offset);

        device->bars[i].mmio    = false;
        device->bars[i].address = 0;
        device->bars[i].size    = 0;

        if (bar_low & 0x1) {
            device->bars[i].mmio    = false;
            device->bars[i].address = bar_low & 0xFFFFFFFC;
        } else {
            device->bars[i].mmio = true;
            uint8_t bar_type     = (bar_low >> 1) & 0x3;

            if (bar_type == 0x0) {
                device->bars[i].address = bar_low & 0xFFFFFFF0;

                uint32_t original_value = device->op->read(bus, equipment, f, 0, offset);

                device->op->write(bus, equipment, f, 0, offset, 0xFFFFFFFF);
                uint32_t value = device->op->read(bus, equipment, f, 0, offset);

                device->op->write(bus, equipment, f, 0, offset, original_value);

                uint32_t mask = (uint32_t)(value & 0xFFFFFFF0);

                device->bars[i].size = (uint64_t)(~mask + 1);
            } else if (bar_type == 0x2) {
                if (i >= 5) {
                    printk("Error: 64-bit BAR at position overflow\n");
                    continue;
                }

                uint32_t bar_high       = device->op->read(bus, equipment, f, 0, offset + 4);
                device->bars[i].address = ((uint64_t)bar_high << 32) | (bar_low & 0xFFFFFFF0);

                uint32_t original_value      = device->op->read(bus, equipment, f, 0, offset);
                uint32_t original_value_high = device->op->read(bus, equipment, f, 0, offset + 4);

                device->op->write(bus, equipment, f, 0, offset, 0xFFFFFFFF);
                device->op->write(bus, equipment, f, 0, offset + 4, 0xFFFFFFFF);
                uint32_t mask      = device->op->read(bus, equipment, f, 0, offset);
                uint32_t mask_high = device->op->read(bus, equipment, f, 0, offset + 4);

                device->op->write(bus, equipment, f, 0, offset, original_value);
                device->op->write(bus, equipment, f, 0, offset + 4, original_value_high);

                uint64_t mask_value = ((uint64_t)mask_high << 32) | (mask & 0xFFFFFFF0);

                device->bars[i].size = ~mask_value + 1;

                i++;

                device->bars[i].mmio    = true;
                device->bars[i].address = 0;
                device->bars[i].size    = 0;
            }
        }
    }

    pci_devices[pci_device_number] = device;
    pci_device_number++;
}

static void pci_config0(uint32_t bus, uint32_t f, uint32_t equipment, uint32_t adder) {
    unsigned int cmd = 0;
    cmd = 0x80000000 + (uint32_t)adder + ((uint32_t)f << 8) + ((uint32_t)equipment << 11)
          + ((uint32_t)bus << 16);
    io_out32(PCI_COMMAND_PORT, cmd);
}

void arch_pci_legacy_enum() {
    // Scan PCI bus
    uint32_t BUS, Equipment, F;
    for (BUS = 0; BUS < 256; BUS++) {
        for (Equipment = 0; Equipment < 32; Equipment++) {
            for (F = 0; F < 8; F++) {
                pci_config0(BUS, F, Equipment, 0);
                if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                    pci_scan_device_legacy(BUS, Equipment, F);
                }
            }
        }
    }
}
