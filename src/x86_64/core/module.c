#include "module.h"
#include "klog.h"
#include "krlibc.h"

cp_module_t module_ls[256];
int         module_count = 0;

LIMINE_REQUEST struct limine_module_request module = {.id = LIMINE_MODULE_REQUEST, .revision = 0};

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

cp_module_t *get_module(const char *module_name) {
    for (int i = 0; i < module_count; i++) {
        if (module_ls[i].is_use && strcmp(module_ls[i].module_name, module_name) == 0) {
            return &module_ls[i];
        }
    }
    return NULL;
}

void module_setup() {
    if (module.response == NULL || module.response->module_count == 0) { return; }

    for (size_t i = 0; i < module.response->module_count; i++) {
        struct limine_file *file = module.response->modules[i];
        extract_name(file->path, module_ls[module_count].module_name, sizeof(char) * 20);
        module_ls[module_count].path   = file->path;
        module_ls[module_count].data   = file->address;
        module_ls[module_count].size   = file->size;
        module_ls[module_count].is_use = true;
        logkf("Module %s %s\n", module_ls[module_count].module_name, file->path);
        module_count++;
    }
}
