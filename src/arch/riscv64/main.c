#include "krlibc.h"
#include "sbi.h"

USED _Noreturn void kmain() {
    while (true) arch_wait_for_interrupt();
}