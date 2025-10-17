#include "bootarg.h"
#include "driver/gop.h"
#include "driver/tty.h"
#include "term/klog.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"

USED _Noreturn void kmain() {
    size_t boot_argc = boot_parse_cmdline(get_kernel_cmdline());
    init_frame();
    init_page();
    init_heap();
    arch_page_setup_l2();
    init_tty();
    init_gop();
    init_tty_session();

    printk("CoolPotOS %s\n",KERNEL_NAME);
    while (true) arch_wait_for_interrupt();
}