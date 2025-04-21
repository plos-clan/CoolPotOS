#include "dlinker.h"
#include "heap.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "timer.h"

extern dlfunc_t __ksymtab_start[]; // .ksymtab section
extern dlfunc_t __ksymtab_end[];
size_t          dlfunc_count = 0;

dlfunc_t __printf = {.name = "printf", .addr = (void *)cp_printf};

EXPORT_SYMBOL(malloc);
EXPORT_SYMBOL(free);
EXPORT_SYMBOL(get_hour);
EXPORT_SYMBOL(get_min);

dlfunc_t *find_func(const char *name) {
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
        if (strcmp(entry->name, name) == 0) { return entry; }
        if (strcmp(name, "printf") == 0) { return &__printf; }
    }
    return NULL;
}

void find_kernel_symbol() {
    dlfunc_count = __ksymtab_end - __ksymtab_start;
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
        logkf("Exported: %s at %p\n", entry->name, entry->addr);
    }
}
