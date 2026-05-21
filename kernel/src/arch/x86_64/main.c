
#include "x86_64.h"

_Noreturn void kmain() {
    while (true)
        __asm__("hlt");
}
