#include "fs/sysfs.h"
#include "krlibc.h"
#include "task/smp.h"

typedef enum sysfs_system_attr {
    SYSFS_SYSTEM_ATTR_CPU_ONLINE,
    SYSFS_SYSTEM_ATTR_CPU_POSSIBLE,
    SYSFS_SYSTEM_ATTR_CPU_PRESENT,
    SYSFS_SYSTEM_ATTR_CPU_NODE_ONLINE,
} sysfs_system_attr_t;

typedef struct sysfs_system_file {
    sysfs_handle_t handle;
    sysfs_system_attr_t attr;
    size_t cpu_id;
} sysfs_system_file_t;

static vfs_node_t sysfs_devices_system_cpu = NULL;
static bool sysfs_system_cpu_ready         = false;

static size_t sysfs_system_cpu_count() {
    size_t cpu_count = get_cpu_count();
    return cpu_count == 0 ? 1 : cpu_count;
}

static size_t sysfs_system_format_attr(
    char *buffer, size_t buffer_size, sysfs_system_attr_t attr, size_t cpu_id
) {
    size_t cpu_count = sysfs_system_cpu_count();

    switch (attr) {
    case SYSFS_SYSTEM_ATTR_CPU_ONLINE:
    case SYSFS_SYSTEM_ATTR_CPU_POSSIBLE:
    case SYSFS_SYSTEM_ATTR_CPU_PRESENT:
        if (cpu_count <= 1) {
            strncpy(buffer, "0\n", buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        } else {
            snprintf(buffer, buffer_size, "0-%zu\n", cpu_count - 1);
        }
        break;
    case SYSFS_SYSTEM_ATTR_CPU_NODE_ONLINE:
        snprintf(buffer, buffer_size, "%d\n", cpu_id < cpu_count ? 1 : 0);
        break;
    default:
        return 0;
    }

    return strlen(buffer);
}

static size_t sysfs_system_attr_read(void *file, void *addr, size_t offset, size_t size) {
    sysfs_system_file_t *handle = file;
    if (handle == NULL || addr == NULL) {
        return 0;
    }

    char content[32];
    size_t len = sysfs_system_format_attr(content, sizeof(content), handle->attr, handle->cpu_id);
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

static size_t sysfs_system_attr_write(void *file, const void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return 0;
}

static void append_system_attr_file(
    vfs_node_t parent, const char *name, sysfs_system_attr_t attr, size_t cpu_id
) {
    vfs_node_t node = sysfs_child_append(parent, name, SYSFS_NONE);
    asserts(node, "sys_devices_system: attr node is null.");

    free(node->handle);

    sysfs_system_file_t *handle = calloc(1, sizeof(sysfs_system_file_t));
    strncpy(handle->handle.name, name, sizeof(handle->handle.name) - 1);
    handle->handle.header.node  = node;
    handle->handle.header.type  = SYSFS_NONE;
    handle->handle.header.read  = sysfs_system_attr_read;
    handle->handle.header.write = sysfs_system_attr_write;
    handle->attr                = attr;
    handle->cpu_id              = cpu_id;

    char content[32];
    node->size   = sysfs_system_format_attr(content, sizeof(content), attr, cpu_id);
    node->handle = handle;
}

static void append_cpu_node(vfs_node_t cpu_root, vfs_node_t bus_cpu_devices, size_t cpu_id) {
    char name[16];
    snprintf(name, sizeof(name), "cpu%zu", cpu_id);

    vfs_node_t cpu_dir = sysfs_ensure_dir(cpu_root, name);
    if (cpu_dir == NULL) {
        return;
    }

    append_system_attr_file(cpu_dir, "online", SYSFS_SYSTEM_ATTR_CPU_NODE_ONLINE, cpu_id);
    sysfs_child_append_symlink(cpu_dir, "subsystem", "/sys/bus/cpu");
    sysfs_child_append_symlink_node(bus_cpu_devices, name, cpu_dir);
}

void sysfs_load_devices_system() {
    vfs_node_t devices_root = sysfs_get_devices_root();
    vfs_node_t bus_root     = sysfs_get_bus_root();

    if (devices_root == NULL || bus_root == NULL) {
        return;
    }

    vfs_node_t system_root = sysfs_ensure_dir(devices_root, "system");
    if (system_root == NULL) {
        return;
    }

    sysfs_devices_system_cpu = sysfs_ensure_dir(system_root, "cpu");
    sysfs_ensure_dir(system_root, "memory");
    sysfs_ensure_dir(system_root, "node");

    vfs_node_t bus_cpu = sysfs_ensure_dir(bus_root, "cpu");
    if (bus_cpu == NULL) {
        return;
    }

    sysfs_ensure_dir(bus_cpu, "devices");
    sysfs_ensure_dir(bus_cpu, "drivers");
}

void sysfs_refresh_devices_system() {
    if (sysfs_system_cpu_ready || sysfs_devices_system_cpu == NULL) {
        return;
    }

    vfs_node_t bus_cpu = sysfs_ensure_dir(sysfs_get_bus_root(), "cpu");
    if (bus_cpu == NULL) {
        return;
    }

    vfs_node_t bus_cpu_devices = sysfs_ensure_dir(bus_cpu, "devices");
    if (bus_cpu_devices == NULL) {
        return;
    }

    append_system_attr_file(sysfs_devices_system_cpu, "online", SYSFS_SYSTEM_ATTR_CPU_ONLINE, 0);
    append_system_attr_file(
        sysfs_devices_system_cpu, "possible", SYSFS_SYSTEM_ATTR_CPU_POSSIBLE, 0
    );
    append_system_attr_file(
        sysfs_devices_system_cpu, "present", SYSFS_SYSTEM_ATTR_CPU_PRESENT, 0
    );

    for (size_t cpu_id = 0; cpu_id < sysfs_system_cpu_count(); cpu_id++) {
        append_cpu_node(sysfs_devices_system_cpu, bus_cpu_devices, cpu_id);
    }

    sysfs_system_cpu_ready = true;
}
