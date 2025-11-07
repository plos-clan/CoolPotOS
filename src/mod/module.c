#include "mod/module.h"
#include "boot.h"
#include "krlibc.h"
#include "mem/heap.h"

module_t boot_modules[MAX_LOAD_MODULE];
size_t modules_count = 0;

void extract_name(const char *input, char *output, size_t output_size) {
    const char *name = strrchr(input, '/');
    if (!name) { return; }
    name++;
    const char *dot = strchr(name, '.');
    if (dot) {
        size_t len = dot - name;
        if (len >= output_size) { len = output_size - 1; }
        strncpy(output, name, len);
        output[len] = '\0';
    } else {
        strncpy(output, name, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

module_t *get_module(const char *module_name) {
    if(module_name == NULL) return NULL;
    for (size_t i = 0; i < modules_count; i++) {
        if (strcmp(boot_modules[i].name, module_name) == 0) {
            return &boot_modules[i];
        }
    }
    return NULL;
}

module_t *get_module_raw(const char *module_name) {
    if(module_name == NULL) return NULL;
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
        memcpy(boot_modules[i].data, boot_modules0[i]->data,
               boot_modules[i].size);
        extract_name(boot_modules[i].path, boot_modules[i].name, sizeof(char) * 20);
    }
}
