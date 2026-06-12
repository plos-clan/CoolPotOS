#include "arch.h"
#include "bootarg.h"
#include "driver/tty.h"
#include "driver/device.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/slub.h"
#include "term/kprint.h"

_Noreturn void kmain() {
    arch_init();
    init_frame();
    page_init();
    slub_init();

    size_t boot_argc = boot_parse_cmdline(boot_get_cmdline());

    device_init();
    tty_init();

    printk("CoolPotOS %s\n", KERNEL_NAME);
    kinfo("kernel cmdline(%llu): %s", boot_argc, boot_get_cmdline());

    ksuccess("kernel load done!\n");
    while (true)
        arch_wait_for_interrupt();
}
