#include "bootarg.h"
#include "description_table.h"
#include "driver/acpi.h"
#include "driver/blk_device.h"
#include "driver/gop.h"
#include "driver/serial.h"
#include "driver/tty.h"
#include "fs/vfs.h"
#include "hpet.h"
#include "intctl.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "term/klog.h"
#include "apic.h"

__attribute__((used, section(".limine_requests_"
                             "start"))) static volatile LIMINE_REQUESTS_START_MARKER;
LIMINE_REQUEST LIMINE_BASE_REVISION(3);

USED _Noreturn void kmain() {
    size_t boot_argc = boot_parse_cmdline(get_kernel_cmdline());
    init_frame();
    init_page();
    init_heap();
    arch_page_setup_l2();
    init_tty();
    init_gop();
    init_serial();
    init_tty_session();
    printk("CoolPotOS %s\n", KERNEL_NAME);
    kinfo("kernel cmdline(%llu): %s", boot_argc, get_kernel_cmdline());
    gdt_setup();
    idt_setup();
    init_err_handle();
    generic_interrupt_table_init();
    init_block_device_manager();
    vfs_init();
    intctl_init();
    acpi_init();
    hpet_init();
    apic_init();
    ksuccess("Kernel load done!");
    while (true)
        arch_wait_for_interrupt();
}