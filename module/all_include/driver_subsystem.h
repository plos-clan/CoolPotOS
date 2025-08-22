#pragma once

#include "cp_kernel.h"

typedef enum {
    VDISK_BLOCK,
    VDISK_STREAM,
} vdisk_flag_t;

typedef struct vdisk_device {
    size_t (*read_vbuf)(int drive, struct vecbuf *buffer, size_t number, size_t lba);
    size_t (*write_vbuf)(int drive, struct vecbuf *buffer, size_t number, size_t lba);
    size_t (*read)(int drive, uint8_t *buffer, size_t number, size_t lba);
    size_t (*write)(int drive, uint8_t *buffer, size_t number, size_t lba);
    int (*ioctl)(struct vdisk_device *device, size_t req, void *handle);
    int (*poll)(size_t events);
    void *(*map)(int drive, void *addr, uint64_t len);
    int          flag;
    size_t       size;        // 大小
    size_t       sector_size; // 扇区大小
    vdisk_flag_t type;
    char         drive_name[50];
} vdisk; // 块设备

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

int    regist_vdisk(vdisk vd);
void  *driver_phys_to_virt(uint64_t phys_addr);
size_t vdisk_read(size_t lba, size_t number, void *buffer, int drive);
size_t vdisk_write(size_t lba, size_t number, const void *buffer, int drive);

pci_device_t *pci_find_vid_did(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_class(uint32_t class_code);
