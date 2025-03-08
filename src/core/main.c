#include "limine.h"
#include "gop.h"
#include "kprint.h"
#include "terminal.h"
#include "module.h"
#include "hhdm.h"
#include "frame.h"
#include "heap.h"
#include "krlibc.h"
#include "acpi.h"
#include "timer.h"
#include "power.h"
#include "description_table.h"
#include "page.h"
#include "vfs.h"
#include "vdisk.h"
#include "keyboard.h"
#include "cpuid.h"
#include "smp.h"
#include "pcb.h"
#include "pcie.h"
#include "nvme.h"
#include "ahci.h"
#include "ide.h"
#include "devfs.h"
#include "pivfs.h"
#include "shell.h"
#include "iic/iic_core.h"
#include "smbios.h"

// 编译器判断
#if defined(__clang__)
#	define COMPILER_NAME "clang"
#	define STRINGIFY(x) #x
#	define EXPAND(x) STRINGIFY(x)
#	define COMPILER_VERSION EXPAND(__clang_major__.__clang_minor__.__clang_patchlevel__)
#elif defined(__GNUC__)
#	define COMPILER_NAME "gcc"
#	define STRINGIFY(x) #x
#	define EXPAND(x) STRINGIFY(x)
#	define COMPILER_VERSION EXPAND(__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__)
#else
#	error "Unknown compiler"
#endif

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2)

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

extern void error_setup(); //error_handle.c

_Noreturn void cp_shutdown() {
    printk("Shutdown %s...\n", KERNEL_NAME);
    // kill_all_proc();
    power_off();
    while (1);
}

_Noreturn void cp_reset() {
    printk("Rebooting %s...\n", KERNEL_NAME);
    // kill_all_proc();
    power_reset();
    while (1);
}

void kmain(void) {
    init_gop();
    init_serial();
    init_hhdm();
    init_frame();
    init_heap();
    module_setup();
    init_terminal();
    init_tty();
    printk("CoolPotOS %s (%s version %s) (Limine Bootloader) on an x86_64\n", KERNEL_NAME, COMPILER_NAME, COMPILER_VERSION);
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);
	kinfo("SMBIOS %d.%d.0 present.\n",smbios_major_version(), smbios_minor_version());
    gdt_setup();
    idt_setup();
    page_setup();
    error_setup();
    acpi_setup();
    keyboard_setup();
    mouse_setup();
    char *date = get_date_time();
    kinfo("RTC time %s", date);
    free(date);
    vfs_init();
    vdisk_init();
    devfs_setup();
    pcie_init();
    ide_setup();
    //nvme_setup();
    //ahci_setup();
    //xhci_setup();


    pivfs_setup();
    init_pcb();
    smp_setup();
    build_stream_device();
    
    init_iic();

    /*TODO*/ create_kernel_thread(terminal_flush_service, NULL, "TerminalFlush");
    create_kernel_thread((void *) shell_setup, NULL, "KernelShell");
    kinfo("Kernel load Done!");
    //beep();
    enable_scheduler();
    open_interrupt;

    cpu_hlt;
}
