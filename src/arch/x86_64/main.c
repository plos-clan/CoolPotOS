#include "apic.h"
#include "bootarg.h"
#include "description_table.h"
#include "driver/acpi.h"
#include "driver/ahci.h"
#include "driver/blk_device.h"
#include "driver/char/ps2_kbd.h"
#include "driver/gop.h"
#include "driver/input_device.h"
#include "driver/pci/pci.h"
#include "driver/power/power.h"
#include "driver/serial.h"
#include "driver/tty.h"
#include "exec/dlinker.h"
#include "exec/elf_load.h"
#include "fpu.h"
#include "fs/cpio.h"
#include "fs/devtmpfs.h"
#include "fs/pipefs.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "fsgsbase.h"
#include "hpet.h"
#include "intctl.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "module.h"
#include "syscall.h"
#include "task/futex.h"
#include "task/scheduler.h"
#include "task/signal.h"
#include "task/smp.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

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
    init_input_manager();
    init_tty_session();
    printk("CoolPotOS %s\n", KERNEL_NAME);
    kinfo("kernel cmdline(%llu): %s", boot_argc, get_kernel_cmdline());
    gdt_setup();
    idt_setup();
    init_err_handle();
    generic_interrupt_table_init();
    load_module();
    init_block_device_manager();
    fsgsbase_init();
    vfs_init();
    intctl_init();
    acpi_init();
    hpet_init();
    apic_init();
    pci_init();
    acpi_namespace_setup();
    tmpfs_regist();
    devtmpfs_regist();
    pipefs_regist();

    ps2_kdb_setup();
    rtc_setup();
    ahci_setup();

    extern intctl_t apic_controller;
    irq_regist_irq(timer, scheduler_handler, 0, NULL, &apic_controller, "sched_handle");
    signal_init();
    futex_init();
    setup_task();
    smp_init();
    arch_enable_syscall();
    float_processor_setup();
    calibrate_tsc_with_hpet();
    power_button_init();
    kmodule_init();
    cpio_init();
    ksuccess("Kernel load done!");
    arch_open_interrupt();
    enable_scheduler();
    start_all_kernel_module();
    launch_init_process();
    while (true)
        arch_wait_for_interrupt();
}