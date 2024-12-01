#ifndef CRASHPOWEROS_PCI_H
#define CRASHPOWEROS_PCI_H

#define PCI_BASE_ADDR0	0x10
#define PCI_BASE_ADDR1	0x14
#define PCI_BASE_ADDR2	0x18
#define PCI_BASE_ADDR3	0x1c
#define PCI_BASE_ADDR4	0x20
#define PCI_BASE_ADDR5	0x24

#define PCI_CONF_VENDOR		0X0 // Vendor ID
#define PCI_CONF_DEVICE		0X2 // Device ID
#define PCI_CONF_COMMAND	0x4 // Command
#define PCI_CONF_STATUS		0x6 // Status
#define PCI_CONF_REVISION	0x8 // Revision ID

#define PCI_COMMAND_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC
#define PCI_DEVICE_MAX 256
#define mem_mapping 0
#define input_output 1

#include <stdint.h>

typedef struct pci_device{
    char* name;
    uint32_t class_code;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
}pci_device_t;

typedef struct base_address_register {
    int prefetchable;
    uint8_t* address;
    uint32_t size;
    int type;
} base_address_register;

struct pci_config_space_public {
    unsigned short VendorID;
    unsigned short DeviceID;
    unsigned short Command;
    unsigned short Status;
    unsigned char RevisionID;
    unsigned char ProgIF;
    unsigned char SubClass;
    unsigned char BaseClass;
    unsigned char CacheLineSize;
    unsigned char LatencyTimer;
    unsigned char HeaderType;
    unsigned char BIST;
    unsigned int BaseAddr[6];
    unsigned int CardbusCIS;
    unsigned short SubVendorID;
    unsigned short SubSystemID;
    unsigned int ROMBaseAddr;
    unsigned char CapabilitiesPtr;
    unsigned char Reserved[3];
    unsigned int Reserved1;
    unsigned char InterruptLine;
    unsigned char InterruptPin;
    unsigned char MinGrant;
    unsigned char MaxLatency;
};

uint32_t pci_read_command_status(uint8_t bus, uint8_t slot, uint8_t func);
void PCI_GET_DEVICE(uint16_t vendor_id, uint16_t device_id, uint8_t* bus, uint8_t* slot, uint8_t* func);
uint32_t pci_get_port_base(uint8_t bus, uint8_t slot, uint8_t func);
uint8_t pci_get_drive_irq(uint8_t bus, uint8_t slot, uint8_t func);
void pci_write_command_status(uint8_t bus, uint8_t slot, uint8_t func, uint32_t value);
void write_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset, uint32_t value);
uint32_t read_bar_n(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_n);
uint32_t read_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset);
base_address_register get_base_address_register(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar);
void pci_config(unsigned int bus, unsigned int f, unsigned int equipment, unsigned int adder);
char *pci_classname(uint32_t classcode);
void load_pci_device(uint32_t BUS,uint32_t Equipment,uint32_t F);
void print_all_pci_info();
pci_device_t *pci_find_class(uint32_t class_code);
uint32_t pci_dev_read32(pci_device_t* pdev, uint16_t offset);
base_address_register find_bar(pci_device_t *device,uint8_t barNum);
void init_pci();

#endif
