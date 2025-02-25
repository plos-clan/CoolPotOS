#pragma once

#define PCI_MCFG_MAX_ENTRIES_LEN 512

#define PCI_COMMAND_IO 0x001
#define PCI_COMMAND_INT_DISABLE 0x400
#define PCI_COMMAND_MEMORY 0x002
#define PCI_COMMAND_MASTER 0x004

#include "acpi.h"

typedef struct{
    uint64_t address;
    bool mmio;
} pcie_bar_base_address;

typedef struct{
    const char *name;
    uint32_t class_code;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t segment;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    pcie_bar_base_address bars[6];
} pcie_device_t;

void pcie_init();
pcie_device_t *pcie_find_class(uint32_t class_code);
void mcfg_addr_to_entries(MCFG_ENTRY **entries);
void print_all_pcie();
uint32_t get_pcie_num();
uint32_t pcie_read_command(pcie_device_t *device, uint8_t offset);
void pcie_write_command(pcie_device_t *device,uint8_t offset,uint32_t value);
