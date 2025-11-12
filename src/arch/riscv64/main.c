#include "bootarg.h"
#include "driver/blk_device.h"
#include "driver/gop.h"
#include "driver/input_device.h"
#include "driver/serial.h"
#include "driver/tty.h"
#include "fs/devtmpfs.h"
#include "fs/pipefs.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "intctl.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "sbi.h"
#include "task/futex.h"
#include "term/klog.h"
#include "rv64_irq.h"

USED _Noreturn void kmain() {
    size_t boot_argc = boot_parse_cmdline(get_kernel_cmdline());
    init_frame();
    init_page();
    init_heap();
    arch_page_setup_l2();
    init_tty();
    init_gop();
    init_serial();
    init_input_manager();
    init_tty_session();
    printk("CoolPotOS %s\n", KERNEL_NAME);
    kinfo("kernel cmdline(%llu): %s", boot_argc, get_kernel_cmdline());
    init_block_device_manager();
    vfs_init();
    intctl_init();
    trap_init();

    tmpfs_regist();
    devtmpfs_regist();
    pipefs_regist();

    signal_init();
    futex_init();
    setup_task();
    ksuccess("Kernel load done!");
    arch_open_interrupt();
    while (true) arch_wait_for_interrupt();
}