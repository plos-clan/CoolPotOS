#pragma once

#define PCI_MCFG_MAX_ENTRIES_LEN 1024
#define PCI_DEVICE_MAX           256

#define PCI_CONF_VENDOR   0X0 // Vendor ID
#define PCI_CONF_DEVICE   0X2 // Device ID
#define PCI_CONF_COMMAND  0x4 // Command
#define PCI_CONF_STATUS   0x6 // Status
#define PCI_CONF_REVISION 0x8 // revision ID

#define EXPORT_BYTE(target, first) ((first) ? ((target) & ~0xFF00) : (((target) & ~0x00FF) >> 8))

#include "types.h"

typedef struct {
    uint64_t address;
    uint64_t size;
    bool mmio;
} pci_bar_t;

typedef struct {
    uint32_t (*read)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);
    void (*write)(
        uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t value
    );
} pci_device_op_t;

typedef struct {
    const char *name;
    uint32_t class_code;
    uint8_t header_type;

    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_device_id;
    uint8_t revision_id;
    uint16_t segment;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    pci_bar_t bars[6];

    uint32_t capability_point;

    uint64_t msix_mmio_vaddr;
    uint64_t msix_mmio_size;
    uint32_t msix_offset;
    uint16_t msix_table_size;

    uint8_t irq_line;
    uint8_t irq_pin;

    pci_device_op_t *op;

    void *desc;
} pci_device_t;

#if defined(__x86_64__) || defined(__amd64__)
uint32_t pci_read0(uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset);
void pci_write0(
    uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset, uint32_t value
);
#endif

const char *pci_classname(uint32_t classcode);
void pci_find_vid(uint32_t vid, void (*load_device)(pci_device_t *device));
void pci_find_class(uint32_t class_code, void (*load_device)(pci_device_t *device));
pci_device_t *pci_find_bdfs(uint8_t bus, uint8_t slot, uint8_t func, uint16_t segment);

uint32_t pci_enumerate_capability_list(pci_device_t *pci_dev, uint32_t cap_type);
void arch_pci_legacy_enum(); // 架构具体实现: MCFG找不到情况下采用经典枚举办法

void pci_scan_bus(uint16_t segment_group, uint8_t bus);
void pci_init();

uint32_t pci_read(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset);
void pci_write(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset, uint32_t value);
