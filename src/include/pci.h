#ifndef CRASHPOWEROS_PCI_H
#define CRASHPOWEROS_PCI_H

#define PCI_COMMAND_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC
#define mem_mapping 0
#define input_output 1

#include <stdint.h>

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
void init_pci();

#endif
