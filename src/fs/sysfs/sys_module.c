#include "fs/sysfs.h"
#include "krlibc.h"
#include "mod/module.h"

typedef enum sysfs_module_attr {
    SYSFS_MODULE_ATTR_CORESIZE,
    SYSFS_MODULE_ATTR_INITSTATE,
    SYSFS_MODULE_ATTR_INITSIZE,
    SYSFS_MODULE_ATTR_REFCNT,
} sysfs_module_attr_t;

typedef struct sysfs_module_file {
    sysfs_handle_t handle;
    module_t *module;
    sysfs_module_attr_t attr;
} sysfs_module_file_t;

static bool module_is_kernel_module(const module_t *mod) {
    if (mod == NULL || mod->path == NULL) {
        return false;
    }

    size_t len = strlen(mod->path);
    return len >= 3 && strcmp(mod->path + len - 3, ".km") == 0;
}

static const char *module_state_name(module_state_t state) {
    switch (state) {
    case M_LOADING:
        return "coming\n";
    case M_RUNNING:
        return "live\n";
    case M_ERROR:
        return "error\n";
    case M_NO_EXEC:
        return "builtin\n";
    default:
        return "unknown\n";
    }
}

static size_t sysfs_mod_read(void *file, void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return 0;
}

static size_t sysfs_mod_write(void *file, const void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return 0;
}

static size_t sysfs_module_attr_read(void *file, void *addr, size_t offset, size_t size) {
    sysfs_module_file_t *attr = file;
    if (attr == NULL || attr->module == NULL || addr == NULL) {
        return 0;
    }

    char content[64];
    switch (attr->attr) {
    case SYSFS_MODULE_ATTR_CORESIZE:
    case SYSFS_MODULE_ATTR_INITSIZE:
        sprintf(content, "%zu\n", attr->module->size);
        break;
    case SYSFS_MODULE_ATTR_INITSTATE:
        strcpy(content, module_state_name(attr->module->state));
        break;
    case SYSFS_MODULE_ATTR_REFCNT:
        strcpy(content, "0\n");
        break;
    default:
        return 0;
    }

    size_t len = strlen(content);
    if (offset >= len) {
        return 0;
    }

    size_t actual = len - offset;
    if (actual > size) {
        actual = size;
    }

    memcpy(addr, content + offset, actual);
    return actual;
}

static size_t sysfs_module_attr_write(void *file, const void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return 0;
}

static void append_module_attr(
    vfs_node_t parent, module_t *mod, const char *name, sysfs_module_attr_t attr
) {
    vfs_node_t node = sysfs_child_append(parent, name, SYSFS_NONE);
    asserts(node, "sys_module: module attr node is null.");

    free(node->handle);

    sysfs_module_file_t *handle = calloc(1, sizeof(sysfs_module_file_t));
    strncpy(handle->handle.name, name, sizeof(handle->handle.name) - 1);
    handle->handle.header.node  = node;
    handle->handle.header.type  = SYSFS_NONE;
    handle->handle.header.read  = sysfs_module_attr_read;
    handle->handle.header.write = sysfs_module_attr_write;
    handle->module              = mod;
    handle->attr                = attr;

    node->handle = handle;
}

static void append_module_node(vfs_node_t root, module_t *mod) {
    vfs_node_t node = sysfs_child_append(root, mod->name, SYSFS_MOD);
    asserts(node, "sys_module: load module node is null.");

    sysfs_module_t *handle = node->handle;
    handle->header.read    = sysfs_mod_read;
    handle->header.write   = sysfs_mod_write;
    handle->module         = mod;

    sysfs_ensure_dir(node, "holders");
    sysfs_ensure_dir(node, "notes");
    sysfs_ensure_dir(node, "parameters");
    sysfs_ensure_dir(node, "sections");

    append_module_attr(node, mod, "coresize", SYSFS_MODULE_ATTR_CORESIZE);
    append_module_attr(node, mod, "initstate", SYSFS_MODULE_ATTR_INITSTATE);
    append_module_attr(node, mod, "initsize", SYSFS_MODULE_ATTR_INITSIZE);
    append_module_attr(node, mod, "refcnt", SYSFS_MODULE_ATTR_REFCNT);

    sysfs_create_file(node, "srcversion", "");
    sysfs_create_file(node, "taint", "");
    sysfs_create_file(node, "uevent", "");
}

void sysfs_load_module(const vfs_node_t node) {
    module_t *mods = get_modules_array();
    for (size_t i = 0; i < get_modules_count(); i++) {
        module_t *mod = &mods[i];
        if (!module_is_kernel_module(mod)) {
            continue;
        }
        append_module_node(node, mod);
    }
}
