#include "dlinker.h"
#include "heap.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"

extern dlfunc_t __ksymtab_start[]; // .ksymtab section
extern dlfunc_t __ksymtab_end[];
size_t          dlfunc_count = 0;

EXPORT_SYMBOL(malloc);
EXPORT_SYMBOL(free);

dlfunc_t *find_func(const char *name) {
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
        if (strcmp(entry->name, name) == 0) { return entry; }
        if (strcmp(entry->name, "printf") == 0) { return (void *)cp_printf; }
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
