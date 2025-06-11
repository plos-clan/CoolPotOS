#include "acpi.h"
#include "ahci.h"
#include "boot.h"
#include "cpuid.h"
#include "description_table.h"
#include "devfs.h"
#include "dlinker.h"
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
#include "module.h"
#include "page.h"
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
#include "vdisk.h"
#include "vfs.h"
#include "xhci.h"

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
extern tcb_t kernel_head_task; // scheduler.c

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
    module_setup();
    init_terminal();

    printk("CoolPotOS %s (git:%s) (%s %s) (%s %s) on an x86_64\n", KERNEL_NAME, GIT_VERSION,
           COMPILER_NAME, COMPILER_VERSION, get_bootloader_name(), get_bootloader_version());
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);
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
    vfs_init();
    tmpfs_setup();
    iso9660_regist();

    vdisk_init();
    devfs_setup();
    init_tty();
    modfs_setup();
    pci_init();
    ide_setup();
    // nvme_setup();
    ahci_setup();
    sb16_init();

    // rtl8169_setup();
    pcnet_setup();
    init_iic();
    disable_scheduler();
    init_pcb();
    smp_setup();
    gop_dev_setup();

    killer_setup();
    setup_syscall(true);
    xhci_setup();
    kinfo("Kernel load Done!");

    create_kernel_thread(terminal_flush_service, NULL, "TerminalFlush", NULL);

    pcb_t shell_group = create_process_group("Shell Service", NULL, NULL, "", NULL, NULL, 0);
    create_kernel_thread((void *)shell_setup, NULL, "KernelShell", shell_group);

    open_interrupt;
    enable_scheduler();

    halt_service();
}
