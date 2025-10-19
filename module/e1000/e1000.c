#include "e1000.h"

void printk(const char *fmt, ...);

__attribute__((used)) __attribute__((visibility("default"))) int dlstart(void) {
    return 0;
}

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    printk("Hello! Module!\n");
    return 0;
}
