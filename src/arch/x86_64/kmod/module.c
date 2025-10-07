#include "dlinker.h"
#include "klog.h"
#include "limine.h"
#include "id_alloc.h"
#include "heap.h"

cp_module_t module_ls[256];
size_t      module_count = 0;

LIMINE_REQUEST struct limine_module_request module = {.id = LIMINE_MODULE_REQUEST, .revision = 0};
LIMINE_REQUEST struct limine_kernel_file_request kfile = {.id       = LIMINE_KERNEL_FILE_REQUEST,
                                                          .response = 0};
id_allocator_t *kmod_allocator = NULL;
kernel_mode_t *kmods[MAX_KERNEL_MODULE];

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

    struct limine_file *kernel = kfile.response->kernel_file;
    strcpy(module_ls[module_count].module_name, "Kernel");
    module_ls[module_count].path   = kernel->path;
    module_ls[module_count].data   = kernel->address;
    module_ls[module_count].size   = kernel->size;
    module_ls[module_count].is_use = true;
    module_count++;

    for (size_t i = 0; i < module.response->module_count; i++) {
        struct limine_file *file = module.response->modules[i];
        extract_name(file->path, module_ls[module_count].module_name, sizeof(char) * 20);
        module_ls[module_count].path = file->path;
        module_ls[module_count].data = file->address;
        module_ls[module_count].size = file->size;
        strcpy(module_ls[module_count].raw_name, file->path);
        module_ls[module_count].is_use = true;
        logkf("kmod: Module %s %s\n", module_ls[module_count].module_name, file->path);
        module_count++;
    }
}

static bool ends_with_km(const char *str) {
    size_t len = strlen(str);
    if (len < 3) return false;
    return strcmp(str + len - 3, ".km") == 0;
}

void start_all_kernel_module() {
    for (size_t i = 0; i < MAX_KERNEL_MODULE; i++) {
        kernel_mode_t *kmod = kmods[i];
        if(kmod == NULL) continue;
        if(kmod->task_entry == NULL) continue;
        int ret = kmod->task_entry();
    }
}

void load_all_kernel_module() {
    kmod_allocator = id_allocator_create(MAX_KERNEL_MODULE);
    for (size_t i = 0; i < module_count; i++) {
        if (module_ls[i].is_use) {
            if (ends_with_km(module_ls[i].raw_name)) {
                logkf("kmod: loading module %s raw: %s\n", module_ls[i].module_name,
                      module_ls[i].raw_name);
                cp_module_t *mod = get_module(module_ls[i].module_name);
                if (mod) {
                    int id = id_alloc(kmod_allocator);
                    kmods[id] = calloc(1,sizeof(kernel_mode_t));
                    dlinker_load(kmods[id],mod);
                }
            }
        }
    }
}
