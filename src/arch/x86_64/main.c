#include "krlibc.h"
#include "io.h"

USED _Noreturn void kmain() {
    while (true) arch_wait_for_interrupt();
}