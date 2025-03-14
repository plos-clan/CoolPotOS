#pragma once

#define PCI_CONF_VENDOR		0X0 // Vendor ID
#define PCI_CONF_DEVICE		0X2 // Device ID
#define PCI_CONF_COMMAND	0x4 // Command
#define PCI_CONF_STATUS		0x6 // Status
#define PCI_CONF_REVISION	0x8 // Revision ID

#define PCI_CAPABILITY_LIST 0x34
#define PCI_CAP_LIST_ID 0x00
#define PCI_CAP_LIST_NEXT 0x01
#define PCI_CAP_ID_MSI 0x05

#define PCI_COMMAND_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC
#define PCI_DEVICE_MAX 256
#define mem_mapping 0
#define input_output 1

#define PCI_RCMD_DISABLE_INTR (1 << 10)
#define  PCI_RCMD_BUS_MASTER (1 << 2)
#define PCI_RCMD_IO_ACCESS 1
#define PCI_RCMD_MM_ACCESS 2

#include "ctype.h"

struct pci_device{
    char* name;
    uint32_t class_code;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t irq_pin;
    uint16_t msi_offset;
};

typedef struct base_address_register {
    int prefetchable;
    uint8_t* address;
    uint32_t size;
    int type;
} base_address_register;

typedef struct pci_device *pci_device_t;

char *pci_classname(uint32_t classcode);
pci_device_t pci_find_class(uint32_t class_code);
pci_device_t pci_find_vid_did(uint16_t vendor_id, uint16_t device_id);
uint32_t read_pci(pci_device_t device, uint8_t registeroffset);
void write_pci(pci_device_t device,uint8_t offset, uint32_t value);
base_address_register find_bar(pci_device_t device, uint8_t barNum);
uint32_t read_bar_n(pci_device_t device, uint8_t bar_n);
void pci_write_command_status(pci_device_t device, uint32_t value);
uint32_t pci_read_command_status(pci_device_t device);
void print_all_pci();
uint32_t get_pci_num();
bool pci_bar_present(pci_device_t device,uint8_t bar);
int pci_find_capability(pci_device_t device, uint8_t cap_id);
void pci_set_msi(pci_device_t device,uint8_t vector);
uint8_t pci_get_drive_irq(uint8_t bus, uint8_t slot, uint8_t func);
void pci_setup();
