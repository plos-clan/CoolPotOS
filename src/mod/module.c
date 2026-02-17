#include "mod/module.h"
#include "boot.h"
#include "errno.h"
#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "term/klog.h"

module_t boot_modules[MAX_LOAD_MODULE];
size_t modules_count = 0;

void extract_name(const char *input, char *output, size_t output_size) {
    const char *name = strrchr(input, '/');
    if (!name) {
        return;
    }
    name++;
    const char *dot = strchr(name, '.');
    if (dot) {
        size_t len = dot - name;
        if (len >= output_size) {
            len = output_size - 1;
        }
        strncpy(output, name, len);
        output[len] = '\0';
    } else {
        strncpy(output, name, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

module_t *get_module(const char *module_name) {
    if (module_name == NULL)
        return NULL;
    for (size_t i = 0; i < modules_count; i++) {
        if (strcmp(boot_modules[i].name, module_name) == 0) {
            return &boot_modules[i];
        }
    }
    return NULL;
}

module_t *get_module_raw(const char *module_name) {
    if (module_name == NULL)
        return NULL;
    for (size_t i = 0; i < modules_count; i++) {
        if (strcmp(boot_modules[i].path, module_name) == 0) {
            return &boot_modules[i];
        }
    }
    return NULL;
}

void load_module() {
    boot_module_t *boot_modules0[MAX_LOAD_MODULE];
    boot_get_modules(boot_modules0, &modules_count);

    for (uint64_t i = 0; i < modules_count; i++) {
        boot_modules[i].path = strdup(boot_modules0[i]->path);
        boot_modules[i].size = boot_modules0[i]->size;
        boot_modules[i].data = malloc(boot_modules[i].size);
        memcpy(boot_modules[i].data, boot_modules0[i]->data, boot_modules[i].size);
        extract_name(boot_modules[i].path, boot_modules[i].name, sizeof(char) * 20);
    }
}

void mount_modfs() {
    vfs_node_t mod = vfs_open("/mod");
    if (mod == NULL) {
        vfs_mkdir("/mod");
        mod = vfs_open("/mod");
        not_null_assert(mod, "error: cannot create modfs.");
    }
    if (vfs_mount(NULL, "tmpfs", mod) != EOK) {
        return;
    }

    for (size_t i = 0; i < modules_count; i++) {
        char path[50];
        module_t module0 = boot_modules[i];
        sprintf(path, "/mod/%s", module0.name);
        vfs_mkfile(path);
        vfs_node_t mod_node = vfs_open(path);
        if (mod_node == NULL) {
            logkf("modfs: cannot create %s\n\r", path);
            continue;
        }
        vfs_write(mod_node, module0.data, 0, module0.size);
    }
}
