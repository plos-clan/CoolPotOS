#pragma once

#include "fs/vfs.h"
#include "mod/module.h"

typedef enum sysfs_type {
    SYSFS_NONE = 0,
    SYSFS_DIR  = 1,
    SYSFS_MOD  = 2,
} sysfs_type_t;

typedef struct sysfs_header {
    sysfs_type_t type;
    vfs_node_t node;
    vfs_read_t read;
    vfs_write_t write;
} sysfs_header_t;

typedef struct sysfs_handle {
    sysfs_header_t header;
    char name[64];
    char *data;
    size_t size;
    size_t capacity;
} sysfs_handle_t;

typedef struct sysfs_devices {
    sysfs_header_t header;
    vfs_node_t class_link;
} sysfs_devices_t;

typedef struct sysfs_module {
    sysfs_header_t header;
    module_t *module;
} sysfs_module_t;

void sysfs_regist();
vfs_node_t sysfs_get_root();
vfs_node_t sysfs_get_class_root();
vfs_node_t sysfs_get_devices_root();
vfs_node_t sysfs_get_bus_root();
vfs_node_t sysfs_get_dev_root();
vfs_node_t sysfs_get_module_root();
vfs_node_t sysfs_get_dev_char_root();
vfs_node_t sysfs_get_dev_block_root();
vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, sysfs_type_t type);
vfs_node_t sysfs_ensure_dir(vfs_node_t parent, const char *name);
vfs_node_t sysfs_create_file(vfs_node_t parent, const char *name, const char *content);
vfs_node_t sysfs_child_append_symlink(vfs_node_t parent, const char *name, const char *target);
vfs_node_t sysfs_child_append_symlink_node(vfs_node_t parent, const char *name, vfs_node_t target);
vfs_node_t sysfs_regist_dev(
    char type,
    int major,
    int minor,
    const char *real_device_path,
    const char *dev_name,
    const char *uevent_content
);

void sysfs_load_module(vfs_node_t node);
void sysfs_load_devices_system();
void sysfs_refresh_devices_system();
void sysfs_load_devices_pci();
vfs_node_t sysfs_get_pci_device_node(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func);
