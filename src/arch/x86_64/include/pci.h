#pragma once

#define PCI_MCFG_MAX_ENTRIES_LEN 1024

#define PCI_CONF_VENDOR   0x0 // Vendor ID
#define PCI_CONF_DEVICE   0x2 // Device ID
#define PCI_CONF_COMMAND  0x4 // Command
#define PCI_CONF_STATUS   0x6 // Status
#define PCI_CONF_REVISION 0x8 // Revision ID

#define PCI_COMMAND_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

#define PCI_CAPABILITY_LIST 0x34
#define PCI_CAP_LIST_ID     0x00
#define PCI_CAP_LIST_NEXT   0x01
#define PCI_CAP_ID_MSI      0x05

#define PCI_DEVICE_MAX 256
#define mem_mapping    0
#define input_output   1

#define PCI_COMMAND_IO     0x001
#define PCI_COMMAND_MEMORY 0x002
#define PCI_COMMAND_MASTER 0x004

#define PCI_RCMD_DISABLE_INTR (1 << 10)
#define PCI_RCMD_BUS_MASTER   (1 << 2)

#include "acpi.h"
#include "ctype.h"

typedef struct {
    uint64_t address;
    uint64_t size;
    bool     mmio;
} pci_bar_base_address;

typedef struct {
    uint32_t (*read)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

    void (*write)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5,
                  uint32_t value);
} pci_device_op_t;

typedef struct {
    const char *name;
    uint32_t    class_code;
    uint8_t     header_type;

    uint16_t             vendor_id;
    uint16_t             device_id;
    uint16_t             segment;
    uint8_t              bus;
    uint8_t              slot;
    uint8_t              func;
    pci_bar_base_address bars[6];

    uint32_t capability_point;

    pci_device_op_t *op;
} pci_device_t;

extern pci_device_t *pci_devices[PCI_DEVICE_MAX];

void pci_scan_segment(uint16_t segment_group);

void pci_scan_function(uint16_t segment_group, uint8_t bus, uint8_t device, uint8_t function);

void pci_scan_bus(uint16_t segment_group, uint8_t bus);

uint32_t pci_read(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset);

void pci_write(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset, uint32_t value);

/**
 * 根据指定的发行商ID和设备ID查找PCI设备
 * @param vendor_id 发行商ID
 * @param device_id 设备ID
 * @return 返回找到的设备, NULL为
 */
pci_device_t *pci_find_vid_did(uint16_t vendor_id, uint16_t device_id);

/**
 * 根据指定 class code查找PCI设备
 * @param class_code class code
 * @return 返回找到的设备, NULL为未找到
 */
pci_device_t *pci_find_class(uint32_t class_code);

uint32_t pci_enumerate_capability_list(pci_device_t *pci_dev, uint32_t cap_type);

uint32_t pci_read_command(pci_device_t *device, uint8_t offset);

void pci_write_command(pci_device_t *device, uint8_t offset, uint32_t value);

uint32_t get_pci_num();

void pci_init();
