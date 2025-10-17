#include "krlibc.h"
#include "sbi.h"
#include "bootarg.h"
#include "mem/frame.h"

USED _Noreturn void kmain() {
    size_t boot_argc = boot_parse_cmdline(get_kernel_cmdline());
    init_frame();
    while (true) arch_wait_for_interrupt();
}