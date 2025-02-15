#include "pcie.h"
#include "pci.h"
#include "kprint.h"
#include "hhdm.h"
#include "alloc.h"

MCFG_ENTRY *mcfg_entries[PCI_MCFG_MAX_ENTRIES_LEN];
MCFG *mcfg;
uint64_t mcfg_entries_len = 0;
pcie_device_t *pci_devices[PCI_DEVICE_MAX];
uint32_t pci_device_number = 0;
bool is_pcie = false; // 是否是PCIE模式 (CP_Kernel无法加载PCIE驱动时候会切换回默认的PCI)

static uint64_t
get_device_mmio_physical_address(uint16_t segment_group, uint8_t bus, uint8_t device, uint8_t function) {
    for (int i = 0; i < mcfg_entries_len; i++) {
        if (mcfg_entries[i]->pci_segment_group == segment_group) {
            return mcfg_entries[i]->base_address + ((bus - mcfg_entries[i]->start_bus) << 20) + (device << 15) +
                   (function << 12);
        }
    }
    return 0;
}

static uint64_t get_mmio_address(uint32_t pci_address, uint16_t offset) {
    uint16_t segment = (pci_address >> 16) & 0xFFFF;
    uint8_t bus = (pci_address >> 8) & 0xFF;
    uint8_t device = (pci_address >> 3) & 0x1F;
    uint8_t function = pci_address & 0x07;
    return (uint64_t) (phys_to_virt(get_device_mmio_physical_address(segment, bus, device, function)) + offset);
}

static uint32_t
segment_bus_device_functon_to_pci_address(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function) {
    return ((uint32_t) segment << 16) | ((uint32_t) bus << 8) | ((uint32_t) device << 3) | (uint32_t) function;
}

void mcfg_addr_to_entries(MCFG_ENTRY **entries) {
    MCFG_ENTRY *entry = (MCFG_ENTRY *) ((uint64_t) mcfg + sizeof(MCFG));
    uint64_t length = mcfg->Header.Length - sizeof(MCFG);
    mcfg_entries_len = length / sizeof(MCFG_ENTRY);
    for (int i = 0; i < mcfg_entries_len; i++) {
        *(entries + i) = entry + i;
    }
}

void pci_scan_function(uint16_t segment_group, uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, bus, device, function);

    uint64_t id_mmio_addr = get_mmio_address(pci_address, 0x00);
    uint16_t vendor_id = *(uint16_t *) id_mmio_addr;
    if (vendor_id == 0xFFFF) {
        return;
    }
    uint16_t device_id = *(uint16_t *) (id_mmio_addr + 2);

    uint64_t field_mmio_addr = get_mmio_address(pci_address, 0x08);
    uint8_t device_revision = *(uint8_t *) field_mmio_addr;
    uint8_t device_class = *((uint8_t *) field_mmio_addr + 3);
    uint8_t device_subclass = *((uint8_t *) field_mmio_addr + 2);
    uint8_t device_interface = *((uint8_t *) field_mmio_addr + 1);

    uint64_t header_type_mmio_addr = get_mmio_address(pci_address, 0x0c);

    switch (*((uint8_t *) header_type_mmio_addr + 2)) {
        // Endpoint
        case 0x00: {
            pcie_device_t *pci_device = (pcie_device_t *) malloc(sizeof(pcie_device_t));
            uint32_t class_code_24bit = (device_class << 16) | (device_subclass << 8) | device_interface;
            pci_device->class_code = class_code_24bit;
            pci_device->vendor_id = vendor_id;
            pci_device->device_id = device_id;

            pci_device->segment = segment_group;
            pci_device->bus = bus;
            pci_device->slot = device;
            pci_device->func = function;

            for (int i = 0; i < 6; i++) {
                int offset = 0x10 + i * 4;
                uint64_t bars_mmio_address = get_mmio_address(pci_address, offset);
                uint32_t bar = *(uint32_t *) bars_mmio_address;

                if (bar & 0x01) {
                    pci_device->bars[i].address = bar & 0xFFFFFFFC;
                    pci_device->bars[i].mmio = false;
                } else {
                    bool prefetchable = bar & (1 << 3);
                    uint64_t bar_address = bar & 0xFFFFFFF0;
                    uint16_t bit = (bar & ((1 << 3) | (1 << 2) | (1 << 1))) >> 1;

                    if(bit == 0b00){
                        pci_device->bars[i].address = bar & 0xFFFFFFFC;
                        pci_device->bars[i].mmio = true;
                    } else if(bit == 0b10){
                        uint32_t bar_address_upper = *((uint32_t *) bars_mmio_address + 1);
                        bar_address |= ((uint64_t) bar_address_upper << 32);
                        pci_device->bars[i].address = bar_address;
                        pci_device->bars[i].mmio = true;
                    }
                }
            }
            pci_device->name = pci_classname(pci_device->class_code);
            pci_devices[pci_device_number] = pci_device;
            pci_device_number++;
        }
            break;
            // PciPciBridge
        case 0x01:
            // Ignore
            break;
            // CardBusBridge
        case 0x02:
            // Ignore
            break;
        default:
            return;
    }
}

void pci_scan_bus(uint16_t segment_group, uint8_t bus) {
    for (int i = 0; i < 32; i++) {
        pci_scan_function(segment_group, bus, i, 0);
        uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, bus, i, 0);
        uint64_t mmio_addr = get_mmio_address(pci_address, 0x0c);
        if (*(uint32_t *) mmio_addr & (1 << 23)) {
            for (int j = 1; j < 8; j++) {
                pci_scan_function(segment_group, bus, i, j);
            }
        }
    }
}

void pci_scan_segment(uint16_t segment_group) {
    pci_scan_bus(segment_group, 0);
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, 0, 0, 0);
    uint64_t mmio_addr = get_mmio_address(pci_address, 0x0c);
    if (*(uint32_t *) mmio_addr & (1 << 23)) {
        for (int i = 1; i < 8; i++) {
            pci_scan_bus(segment_group, i);
        }
    }
}

uint32_t get_pcie_num() {
    return is_pcie ? pci_device_number : get_pci_num();
}

void print_all_pcie() {
    if(!is_pcie){
        print_all_pci();
        printk("Model: PCI\n");
        return;
    }
    printk("Bus:Slot:Func\t[Vendor:Device]\tClass Code\tName\n");
    for (int i = 0; i < pci_device_number; i++) {
        pcie_device_t *device = pci_devices[i];
        printk("%03d:%02d:%02d\t[0x%04X:0x%04X]\t<0x%08x>\t%s\n",
               device->bus,
               device->slot,
               device->func,
               device->vendor_id,
               device->device_id,
               device->class_code,
               device->name);
    }
    printk("Model: PCIE\n");
}

pcie_device_t *pcie_find_class(uint32_t class_code) {
    for (int i = 0; i < pci_device_number; i++) {
        if (pci_devices[i]->class_code == class_code) {
            return pci_devices[i];
        }
        if (class_code == (pci_devices[i]->class_code & 0xFFFF00)) {
            return pci_devices[i];
        }
    }
    return NULL;
}

void pcie_init() {
    if (mcfg == NULL) {
        pci_setup();
        return;
    }
    mcfg_addr_to_entries(mcfg_entries);
    for (int i = 0; i < mcfg_entries_len; i++) {
        uint16_t segment_group = mcfg_entries[i]->pci_segment_group;
        pci_scan_segment(segment_group);
    }
    is_pcie = true;
    kinfo("PCIE device find %d",pci_device_number);
}

/**
 * ACPI回调设置MCFG表基址
 * @param mcfg0 MCFG表基址(虚拟地址)
 */
void pcie_setup(MCFG *mcfg0) {
    mcfg = mcfg0;
}
