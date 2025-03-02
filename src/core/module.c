#include "module.h"
#include "krlibc.h"
#include "klog.h"

cp_module_t module_ls[256];
int module_count = 0;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

cp_module_t *get_module(const char *module_name) {
    for (int i = 0; i < module_count; i++) {
        if (strcmp(module_ls[i].module_name, module_name) == 0) {
            return &module_ls[i];
        }
    }
    return NULL;
}

void module_setup() {
    if (module.response == NULL || module.response->module_count == 0) {
        return;
    }

    for (int i = 0; i < module.response->module_count; i++) {
        struct limine_file *file = module.response->modules[i];
        logkf("Module %d: %s\n", i, file->path);
        if(!strcmp(file->path,"/sys/sysfont.ttf")){
            logkf("Load sysfont module.\n");
            module_ls[module_count] = (cp_module_t){
                    .is_use = true,
                    .size = file->size,
                    .data = file->address,
            };
            strcpy(module_ls[module_count++].module_name,"sysfont");
        }
    }

}
