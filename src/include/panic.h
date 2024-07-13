#ifndef CRASHPOWEROS_PANIC_H
#define CRASHPOWEROS_PANIC_H

enum PANIC_TYPE{
    ILLEGAL_KERNEL_STATUS,
    OUT_OF_MEMORY,
    KERNEL_PAGE_FAULT
};

void panic_pane(char* msg,enum PANIC_TYPE type);
void init_eh();

#endif
