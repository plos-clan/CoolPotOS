#pragma once

#define PCI_CONF_VENDOR   0X0 // Vendor ID
#define PCI_CONF_DEVICE   0X2 // Device ID
#define PCI_CONF_COMMAND  0x4 // Command
#define PCI_CONF_STATUS   0x6 // Status
#define PCI_CONF_REVISION 0x8 // revision ID

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1) // 0x000FFFFF

#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

#define MAJOR(dev) ((unsigned int)((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int)((dev) & MINORMASK))

#include "cp_kernel.h"

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

typedef struct block_device blk_device_t;

enum blk_type {
    BLK_BLOCK_DEVICE,
    BLK_PARTITION,
    BLK_STREAM_DEVICE,
};

struct block_device_ops {
    size_t (*read)(void *handle, uint8_t *buffer, size_t number, size_t lba);
    size_t (*write)(void *handle, uint8_t *buffer, size_t number, size_t lba);
    int (*ioctl)(blk_device_t *device, size_t req, void *handle);
    int (*poll)(size_t events);
    void *(*map)(void *handle, void *addr, size_t len);
    errno_t (*del_blk)(void *handle);
};

struct hd_geometry {
    unsigned char heads;      // 磁头数 (Heads)
    unsigned char sectors;    // 每磁道的扇区数 (Sectors per track)
    unsigned short cylinders; // 柱面数 (Cylinders)
    unsigned long start;      // 该分区在磁盘上的起始偏移量 (以扇区为单位)
};

struct block_device {
    void *handle;
    size_t device_id;
    size_t size;       // 块设备大小
    size_t block_size; // 块大小
    size_t max_size;   // 最大读取缓冲区
    uint64_t dev;      // 设备号
    char name[20];

    struct hd_geometry geometry;

    enum blk_type type;
    struct block_device_ops ops;
};

void pci_find_class(uint32_t class_code, void (*load_device)(pci_device_t *device));
size_t register_device(blk_device_t *device);
uint64_t nano_time();
