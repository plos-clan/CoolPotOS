#include "extfs.h"
#include "cp_kernel.h"

__attribute__((visibility("default"))) void dlmain() {
    printk("ExtFS module loaded\n");
}
