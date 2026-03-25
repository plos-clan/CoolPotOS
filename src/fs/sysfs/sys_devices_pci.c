#include "driver/pci/pci.h"
#include "fs/sysfs.h"
#include "krlibc.h"

#define SYSFS_PCI_CONFIG_SIZE 256U

typedef enum sysfs_pci_attr {
    SYSFS_PCI_ATTR_VENDOR,
    SYSFS_PCI_ATTR_DEVICE,
    SYSFS_PCI_ATTR_SUBSYSTEM_VENDOR,
    SYSFS_PCI_ATTR_SUBSYSTEM_DEVICE,
    SYSFS_PCI_ATTR_REVISION,
    SYSFS_PCI_ATTR_CLASS,
    SYSFS_PCI_ATTR_IRQ,
    SYSFS_PCI_ATTR_MODALIAS,
    SYSFS_PCI_ATTR_UEVENT,
    SYSFS_PCI_ATTR_CONFIG,
    SYSFS_PCI_ATTR_RESOURCE,
} sysfs_pci_attr_t;

typedef struct sysfs_pci_file {
    sysfs_handle_t handle;
    pci_device_t *device;
    sysfs_pci_attr_t attr;
} sysfs_pci_file_t;

static vfs_node_t sysfs_pci_bus         = NULL;
static vfs_node_t sysfs_pci_bus_devices = NULL;

static char *sysfs_pci_join_path(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *base = vfs_get_fullpath(parent);
    if (base == NULL) {
        return NULL;
    }

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    size_t path_len = base_len + name_len + 2;
    char *path      = malloc(path_len);
    if (path == NULL) {
        free(base);
        return NULL;
    }

    if (strcmp(base, "/") == 0) {
        sprintf(path, "/%s", name);
    } else {
        sprintf(path, "%s/%s", base, name);
    }

    free(base);
    return path;
}

static vfs_node_t sysfs_pci_lookup_child(vfs_node_t parent, const char *name) {
    char *path = sysfs_pci_join_path(parent, name);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t node = vfs_open(path);
    free(path);
    return node;
}

static void sysfs_pci_release_handle(vfs_node_t node) {
    if (node == NULL || node->handle == NULL) {
        return;
    }

    sysfs_header_t *header = node->handle;
    if (header->type == SYSFS_NONE) {
        sysfs_handle_t *handle = node->handle;
        if (handle->data != NULL) {
            free(handle->data);
        }
    }

    free(node->handle);
    node->handle = NULL;
}

static void sysfs_pci_format_root_name(
    const pci_device_t *device, char *buffer, size_t buffer_size
) {
    snprintf(buffer, buffer_size, "pci%04x:%02x", device->segment, device->bus);
}

static void sysfs_pci_format_slot_name(
    const pci_device_t *device, char *buffer, size_t buffer_size
) {
    snprintf(
        buffer,
        buffer_size,
        "%04x:%02x:%02x.%u",
        device->segment,
        device->bus,
        device->slot,
        device->func
    );
}

static size_t sysfs_pci_format_modalias(
    const pci_device_t *device, char *buffer, size_t buffer_size, bool trailing_newline
) {
    uint8_t base_class = (device->class_code >> 16) & 0xff;
    uint8_t subclass   = (device->class_code >> 8) & 0xff;
    uint8_t interface  = device->class_code & 0xff;

    snprintf(
        buffer,
        buffer_size,
        trailing_newline
            ? "pci:v%08xd%08xsv%08xsd%08xbc%02xsc%02xi%02x\n"
            : "pci:v%08xd%08xsv%08xsd%08xbc%02xsc%02xi%02x",
        device->vendor_id,
        device->device_id,
        device->subsystem_vendor_id,
        device->subsystem_device_id,
        base_class,
        subclass,
        interface
    );
    return strlen(buffer);
}

static size_t sysfs_pci_format_uevent(
    const pci_device_t *device, char *buffer, size_t buffer_size
) {
    char slot_name[16];
    char modalias[96];

    sysfs_pci_format_slot_name(device, slot_name, sizeof(slot_name));
    sysfs_pci_format_modalias(device, modalias, sizeof(modalias), false);

    snprintf(
        buffer,
        buffer_size,
        "PCI_CLASS=%06x\nPCI_ID=%04x:%04x\nPCI_SUBSYS_ID=%04x:%04x\nPCI_SLOT_NAME=%s\nMODALIAS=%s\n",
        device->class_code & 0xffffff,
        device->vendor_id,
        device->device_id,
        device->subsystem_vendor_id,
        device->subsystem_device_id,
        slot_name,
        modalias
    );
    return strlen(buffer);
}

static size_t sysfs_pci_format_resource(
    const pci_device_t *device, char *buffer, size_t buffer_size
) {
    size_t total = 0;
    size_t used  = 0;

    if (buffer != NULL && buffer_size > 0) {
        buffer[0] = '\0';
    }

    for (size_t i = 0; i < 6; i++) {
        uint64_t start = device->bars[i].address;
        uint64_t end   = 0;
        uint32_t flags = 0;
        char line[64];

        if (start != 0 && device->bars[i].size != 0) {
            end = start + device->bars[i].size - 1;
        }
        if (start != 0) {
            flags = device->bars[i].mmio ? 0x00000200U : 0x00000100U;
        }

        int len = snprintf(
            line,
            sizeof(line),
            "0x%016lx 0x%016lx 0x%08x\n",
            (unsigned long)start,
            (unsigned long)end,
            flags
        );
        if (len <= 0) {
            continue;
        }

        if (buffer != NULL && buffer_size > 0 && used < buffer_size - 1) {
            size_t copy      = (size_t)len;
            size_t remaining = buffer_size - used - 1;
            if (copy > remaining) {
                copy = remaining;
            }
            memcpy(buffer + used, line, copy);
            used += copy;
            buffer[used] = '\0';
        }

        total += (size_t)len;
    }

    return total;
}

static size_t sysfs_pci_format_text(
    const sysfs_pci_file_t *handle, char *buffer, size_t buffer_size
) {
    if (handle == NULL || handle->device == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    switch (handle->attr) {
    case SYSFS_PCI_ATTR_VENDOR:
        snprintf(buffer, buffer_size, "0x%04x\n", handle->device->vendor_id);
        break;
    case SYSFS_PCI_ATTR_DEVICE:
        snprintf(buffer, buffer_size, "0x%04x\n", handle->device->device_id);
        break;
    case SYSFS_PCI_ATTR_SUBSYSTEM_VENDOR:
        snprintf(buffer, buffer_size, "0x%04x\n", handle->device->subsystem_vendor_id);
        break;
    case SYSFS_PCI_ATTR_SUBSYSTEM_DEVICE:
        snprintf(buffer, buffer_size, "0x%04x\n", handle->device->subsystem_device_id);
        break;
    case SYSFS_PCI_ATTR_REVISION:
        snprintf(buffer, buffer_size, "0x%02x\n", handle->device->revision_id);
        break;
    case SYSFS_PCI_ATTR_CLASS:
        snprintf(buffer, buffer_size, "0x%06x\n", handle->device->class_code & 0xffffff);
        break;
    case SYSFS_PCI_ATTR_IRQ:
        snprintf(buffer, buffer_size, "%u\n", handle->device->irq_line);
        break;
    case SYSFS_PCI_ATTR_MODALIAS:
        return sysfs_pci_format_modalias(handle->device, buffer, buffer_size, true);
    case SYSFS_PCI_ATTR_UEVENT:
        return sysfs_pci_format_uevent(handle->device, buffer, buffer_size);
    case SYSFS_PCI_ATTR_RESOURCE:
        return sysfs_pci_format_resource(handle->device, buffer, buffer_size);
    default:
        return 0;
    }

    return strlen(buffer);
}

static uint32_t sysfs_pci_read_config_dword(const pci_device_t *device, uint32_t offset) {
    if (device == NULL || device->op == NULL || device->op->read == NULL) {
        return 0xffffffffU;
    }

    return device->op->read(device->bus, device->slot, device->func, device->segment, offset);
}

static void sysfs_pci_write_config_dword(const pci_device_t *device, uint32_t offset, uint32_t value) {
    if (device == NULL || device->op == NULL || device->op->write == NULL) {
        return;
    }

    device->op->write(device->bus, device->slot, device->func, device->segment, offset, value);
}

static size_t sysfs_pci_config_read(
    const pci_device_t *device, void *addr, size_t offset, size_t size
) {
    if (device == NULL || addr == NULL || offset >= SYSFS_PCI_CONFIG_SIZE) {
        return 0;
    }

    size_t actual = SYSFS_PCI_CONFIG_SIZE - offset;
    if (actual > size) {
        actual = size;
    }

    uint8_t *buffer      = addr;
    uint32_t cached_base = UINT32_MAX;
    uint32_t cached_val  = 0;

    for (size_t i = 0; i < actual; i++) {
        uint32_t pos  = (uint32_t)(offset + i);
        uint32_t base = pos & ~0x3U;
        if (base != cached_base) {
            cached_val  = sysfs_pci_read_config_dword(device, base);
            cached_base = base;
        }
        buffer[i] = (uint8_t)((cached_val >> ((pos & 0x3U) * 8U)) & 0xffU);
    }

    return actual;
}

static size_t sysfs_pci_config_write(
    const pci_device_t *device, const void *addr, size_t offset, size_t size
) {
    if (device == NULL || addr == NULL || offset >= SYSFS_PCI_CONFIG_SIZE) {
        return 0;
    }

    size_t actual = SYSFS_PCI_CONFIG_SIZE - offset;
    if (actual > size) {
        actual = size;
    }

    const uint8_t *buffer = addr;
    size_t written        = 0;

    while (written < actual) {
        uint32_t pos   = (uint32_t)(offset + written);
        uint32_t base  = pos & ~0x3U;
        uint32_t value = sysfs_pci_read_config_dword(device, base);
        size_t start   = pos & 0x3U;
        size_t chunk   = 4U - start;

        if (chunk > actual - written) {
            chunk = actual - written;
        }

        for (size_t i = 0; i < chunk; i++) {
            uint32_t shift = (uint32_t)((start + i) * 8U);
            value &= ~(0xffU << shift);
            value |= (uint32_t)buffer[written + i] << shift;
        }

        sysfs_pci_write_config_dword(device, base, value);
        written += chunk;
    }

    return actual;
}

static size_t sysfs_pci_attr_size(const sysfs_pci_file_t *handle) {
    if (handle == NULL) {
        return 0;
    }

    if (handle->attr == SYSFS_PCI_ATTR_CONFIG) {
        return SYSFS_PCI_CONFIG_SIZE;
    }

    char content[512];
    return sysfs_pci_format_text(handle, content, sizeof(content));
}

static size_t sysfs_pci_attr_read(void *file, void *addr, size_t offset, size_t size) {
    sysfs_pci_file_t *handle = file;
    if (handle == NULL || handle->device == NULL) {
        return 0;
    }

    if (handle->attr == SYSFS_PCI_ATTR_CONFIG) {
        return sysfs_pci_config_read(handle->device, addr, offset, size);
    }

    char content[512];
    size_t len = sysfs_pci_format_text(handle, content, sizeof(content));
    if (len == 0 || offset >= len) {
        return 0;
    }

    size_t actual = len - offset;
    if (actual > size) {
        actual = size;
    }

    memcpy(addr, content + offset, actual);
    return actual;
}

static size_t sysfs_pci_attr_write(void *file, const void *addr, size_t offset, size_t size) {
    sysfs_pci_file_t *handle = file;
    if (handle == NULL || handle->device == NULL) {
        return 0;
    }

    switch (handle->attr) {
    case SYSFS_PCI_ATTR_CONFIG:
        return sysfs_pci_config_write(handle->device, addr, offset, size);
    case SYSFS_PCI_ATTR_UEVENT:
        return size;
    default:
        return 0;
    }
}

static void sysfs_pci_install_attr_file(
    vfs_node_t parent, const char *name, pci_device_t *device, sysfs_pci_attr_t attr
) {
    vfs_node_t node = sysfs_pci_lookup_child(parent, name);
    if (node == NULL) {
        node = sysfs_child_append(parent, name, SYSFS_NONE);
    }
    if (node == NULL) {
        return;
    }

    sysfs_pci_release_handle(node);

    sysfs_pci_file_t *handle = calloc(1, sizeof(sysfs_pci_file_t));
    strncpy(handle->handle.name, name, sizeof(handle->handle.name) - 1);
    handle->handle.header.node  = node;
    handle->handle.header.type  = SYSFS_NONE;
    handle->handle.header.read  = sysfs_pci_attr_read;
    handle->handle.header.write = sysfs_pci_attr_write;
    handle->device              = device;
    handle->attr                = attr;

    node->handle = handle;
    node->size   = sysfs_pci_attr_size(handle);
    node->mode   = (attr == SYSFS_PCI_ATTR_CONFIG || attr == SYSFS_PCI_ATTR_UEVENT) ? 0644 : 0444;
}

static void sysfs_pci_ensure_symlink(vfs_node_t parent, const char *name, const char *target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (sysfs_pci_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink(parent, name, target);
}

static void sysfs_pci_ensure_symlink_node(vfs_node_t parent, const char *name, vfs_node_t target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (sysfs_pci_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink_node(parent, name, target);
}

static bool sysfs_pci_prepare_bus_roots() {
    if (sysfs_get_devices_root() == NULL || sysfs_get_bus_root() == NULL) {
        return false;
    }

    if (sysfs_pci_bus == NULL) {
        sysfs_pci_bus = sysfs_ensure_dir(sysfs_get_bus_root(), "pci");
    }
    if (sysfs_pci_bus == NULL) {
        return false;
    }

    if (sysfs_pci_bus_devices == NULL) {
        sysfs_pci_bus_devices = sysfs_ensure_dir(sysfs_pci_bus, "devices");
    }
    if (sysfs_pci_bus_devices == NULL) {
        return false;
    }

    sysfs_ensure_dir(sysfs_pci_bus, "drivers");
    return true;
}

static vfs_node_t sysfs_pci_lookup_device_node(
    uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func
) {
    char path[64];
    snprintf(
        path,
        sizeof(path),
        "/sys/devices/pci%04x:%02x/%04x:%02x:%02x.%u",
        segment,
        bus,
        segment,
        bus,
        slot,
        func
    );
    return vfs_open(path);
}

static vfs_node_t sysfs_pci_ensure_device_node(pci_device_t *device) {
    if (device == NULL || !sysfs_pci_prepare_bus_roots()) {
        return NULL;
    }

    vfs_node_t device_node = sysfs_pci_lookup_device_node(
        device->segment, device->bus, device->slot, device->func
    );
    if (device_node == NULL) {
        char root_name[16];
        char slot_name[16];

        sysfs_pci_format_root_name(device, root_name, sizeof(root_name));
        sysfs_pci_format_slot_name(device, slot_name, sizeof(slot_name));

        vfs_node_t bus_root = sysfs_ensure_dir(sysfs_get_devices_root(), root_name);
        if (bus_root == NULL) {
            return NULL;
        }

        device_node = sysfs_ensure_dir(bus_root, slot_name);
        if (device_node == NULL) {
            return NULL;
        }
    }

    char slot_name[16];
    sysfs_pci_format_slot_name(device, slot_name, sizeof(slot_name));

    sysfs_pci_ensure_symlink(device_node, "subsystem", "/sys/bus/pci");
    sysfs_pci_ensure_symlink_node(sysfs_pci_bus_devices, slot_name, device_node);

    sysfs_pci_install_attr_file(device_node, "vendor", device, SYSFS_PCI_ATTR_VENDOR);
    sysfs_pci_install_attr_file(device_node, "device", device, SYSFS_PCI_ATTR_DEVICE);
    sysfs_pci_install_attr_file(
        device_node, "subsystem_vendor", device, SYSFS_PCI_ATTR_SUBSYSTEM_VENDOR
    );
    sysfs_pci_install_attr_file(
        device_node, "subsystem_device", device, SYSFS_PCI_ATTR_SUBSYSTEM_DEVICE
    );
    sysfs_pci_install_attr_file(device_node, "revision", device, SYSFS_PCI_ATTR_REVISION);
    sysfs_pci_install_attr_file(device_node, "class", device, SYSFS_PCI_ATTR_CLASS);
    sysfs_pci_install_attr_file(device_node, "irq", device, SYSFS_PCI_ATTR_IRQ);
    sysfs_pci_install_attr_file(device_node, "modalias", device, SYSFS_PCI_ATTR_MODALIAS);
    sysfs_pci_install_attr_file(device_node, "uevent", device, SYSFS_PCI_ATTR_UEVENT);
    sysfs_pci_install_attr_file(device_node, "config", device, SYSFS_PCI_ATTR_CONFIG);
    sysfs_pci_install_attr_file(device_node, "resource", device, SYSFS_PCI_ATTR_RESOURCE);

    return device_node;
}

void sysfs_load_devices_pci() {
    if (!sysfs_pci_prepare_bus_roots()) {
        return;
    }

    size_t device_count = pci_get_device_count();
    for (size_t i = 0; i < device_count; i++) {
        pci_device_t *device = pci_get_device_by_index(i);
        if (device == NULL) {
            continue;
        }
        sysfs_pci_ensure_device_node(device);
    }
}

vfs_node_t sysfs_get_pci_device_node(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func) {
    vfs_node_t node = sysfs_pci_lookup_device_node(segment, bus, slot, func);
    if (node != NULL) {
        return node;
    }

    pci_device_t *device = pci_find_bdfs(bus, slot, func, segment);
    if (device == NULL) {
        return NULL;
    }

    return sysfs_pci_ensure_device_node(device);
}
