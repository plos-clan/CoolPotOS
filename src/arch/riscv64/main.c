#include "cptype.h"
#include "heap.h"

void kmain() {
    uintptr_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    while (1)
        __asm__ volatile("wfi" ::: "memory");
}
