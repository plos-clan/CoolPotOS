#include "acpi.h"
#include "ahci.h"
#include "boot.h"
#include "cpuid.h"
#include "cpustats.h"
#include "description_table.h"
#include "devfs.h"
#include "device.h"
#include "dlinker.h"
#include "eevdf.h"
#include "frame.h"
#include "fsgsbase.h"
#include "gop.h"
#include "heap.h"
#include "hhdm.h"
#include "ide.h"
#include "iic/iic_core.h"
#include "keyboard.h"
#include "killer.h"
#include "kprint.h"
#include "krlibc.h"
#include "modfs.h"
#include "network.h"
#include "page.h"
#include "partition.h"
#include "pcb.h"
#include "pci.h"
#include "pcnet.h"
#include "power.h"
#include "sb16.h"
#include "shell.h"
#include "smbios.h"
#include "smp.h"
#include "syscall.h"
#include "sysuser.h"
#include "terminal.h"
#include "timer.h"
#include "tmpfs.h"
#include "vfs.h"

// 编译器判断
#if defined(__clang__)
#    define COMPILER_NAME    "clang"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__clang_major__.__clang_minor__.__clang_patchlevel__)
#elif defined(__GNUC__)
#    define COMPILER_NAME    "gcc"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__)
#else
#    error "Unknown compiler"
#endif

// Git哈希判断
#ifndef GIT_VERSION
#    define GIT_VERSION "unknown"
#endif

void kmain();

extern void  error_setup();    // error_handle.c
extern void  iso9660_regist(); // iso9660.c
extern void  fatfs_init();     // fatfs.c
extern void  pipefs_setup();   // pipefs.c
extern tcb_t kernel_head_task; // scheduler.c
extern void  setup_urandom();  // urandom.c
extern void  zero_setup();     // zero.c

_Noreturn void cp_shutdown() {
    printk("Shutdown %s...\n", KERNEL_NAME);
    power_off();
    loop;
}

_Noreturn void cp_reset() {
    printk("Rebooting %s...\n", KERNEL_NAME);
    power_reset();
    loop;
}

void kmain() {
    gdt_setup();
    idt_setup();
    init_gop();
    init_serial();
    init_hhdm();
    init_frame();
    page_setup();
    init_heap();
    init_terminal();
    init_tty();
    printk("CoolPotOS %s (git:%s) (%s %s) (%s %s) on an x86_64\n", KERNEL_NAME, GIT_VERSION,
           COMPILER_NAME, COMPILER_VERSION, get_bootloader_name(), get_bootloader_version());
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);
    kinfo("DMI: %s %s, BIOS %s %s", smbios_sys_manufacturer(), smbios_sys_product_name(),
          smbios_bios_version(), smbios_bios_release_date());
    module_setup();
    error_setup();
    float_processor_setup();
    fsgsbase_init();
    dlinker_init();
    user_setup();
    smbios_setup();
    acpi_setup();
    keyboard_setup();
    mouse_setup();
    char *date = get_date_time();
    kinfo("RTC time %s", date);
    free(date);
    calibrate_tsc_with_hpet();
    vfs_init();
    tmpfs_setup();
    iso9660_regist();
    pipefs_setup();
    fatfs_init();

    device_manager_init();
    devfs_setup();
    build_tty_device();
    modfs_setup();
    pci_init();
    ide_setup();
    ahci_setup();
    sb16_init();

    pcnet_setup();
    init_iic();
    disable_scheduler();
    signal_init();
    init_pcb();
    smp_setup();
    gop_dev_setup();
    setup_urandom();
    zero_setup();
    killer_setup();
    setup_syscall(true);
    partition_init();
    netfs_setup();
    load_all_kernel_module();
    kinfo("Kernel load Done!");

    pcb_t shell_group = create_process_group("Shell Service", NULL, NULL, "", NULL, NULL, 0);
    create_kernel_thread((void *)shell_setup, NULL, "KernelShell", shell_group, SIZE_MAX);

    open_interrupt;
    enable_scheduler();
    change_bsp_weight();

    loop __asm__ volatile("hlt");
}
