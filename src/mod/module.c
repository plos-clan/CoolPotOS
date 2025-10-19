#include "module.h"
#include "krlibc.h"
#include "limine.h"
#include "mem/heap.h"

LIMINE_REQUEST struct limine_module_request modules_request = {
    .id       = LIMINE_MODULE_REQUEST,
    .revision = 0,
};

module_t boot_modules[256];
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

void load_module() {
    for (uint64_t i = 0; i < modules_request.response->module_count; i++) {
        boot_modules[i].path = strdup(modules_request.response->modules[i]->path);
        boot_modules[i].size = modules_request.response->modules[i]->size;
        boot_modules[i].data = malloc(boot_modules[i].size);
        memcpy(boot_modules[i].data, modules_request.response->modules[i]->address,
               boot_modules[i].size);
        extract_name(boot_modules[i].path, boot_modules[i].name, sizeof(char) * 20);
    }
    modules_count = modules_request.response->module_count;
}
