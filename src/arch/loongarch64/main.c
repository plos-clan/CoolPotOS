#include "krlibc.h"

USED _Noreturn void kmain() {
    for (;;)
        arch_wait_for_interrupt();
}
