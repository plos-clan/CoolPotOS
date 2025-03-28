#include "acpi.h"
#include "ahci.h"
#include "cpuid.h"
#include "description_table.h"
#include "devfs.h"
#include "frame.h"
#include "gop.h"
#include "heap.h"
#include "hhdm.h"
#include "ide.h"
#include "iic/iic_core.h"
#include "keyboard.h"
#include "kprint.h"
#include "krlibc.h"
#include "limine.h"
#include "module.h"
#include "nvme.h"
#include "page.h"
#include "pcb.h"
#include "pcie.h"
#include "dlinker.h"
#include "pivfs.h"
#include "power.h"
#include "rtl8169.h"
#include "shell.h"
#include "smbios.h"
#include "smp.h"
#include "terminal.h"
#include "timer.h"
#include "vdisk.h"
#include "vfs.h"
#include "pcnet.h"
#include "cpusp.h"

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

USED SECTION(".limine_requests_start") static const volatile LIMINE_REQUESTS_START_MARKER;

USED SECTION(".limine_requests_end") static const volatile LIMINE_REQUESTS_END_MARKER;

LIMINE_REQUEST LIMINE_BASE_REVISION(2);

LIMINE_REQUEST struct limine_stack_size_request stack_request = {
    .id         = LIMINE_STACK_SIZE_REQUEST,
    .revision   = 0,
    .stack_size = KERNEL_ST_SZ // 128K
};

extern void  error_setup();    // error_handle.c
extern void  iso9660_regist(); // iso9660.c
extern tcb_t kernel_head_task; // scheduler.c

_Noreturn void cp_shutdown() {
    printk("Shutdown %s...\n", KERNEL_NAME);
    power_off();
    infinite_loop;
}

_Noreturn void cp_reset() {
    printk("Rebooting %s...\n", KERNEL_NAME);
    power_reset();
    infinite_loop;
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
    printk("CoolPotOS %s (%s version %s) (Limine Bootloader) on an x86_64\n", KERNEL_NAME,
           COMPILER_NAME, COMPILER_VERSION);
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);
    gdt_setup();
    idt_setup();
    page_setup();
    error_setup();
    float_processor_setup();
    dlinker_init();
    smbios_setup();
    acpi_setup();
    keyboard_setup();
    mouse_setup();
    char *date = get_date_time();
    kinfo("RTC time %s", date);
    free(date);
    vfs_init();
    iso9660_regist();

    vdisk_init();
    devfs_setup();
    pcie_init();
    ide_setup();
    // nvme_setup();
    ahci_setup();
    // xhci_setup();

    // rtl8169_setup();
    pcnet_setup();
    disable_scheduler();
    pivfs_setup();
    init_pcb();
    smp_setup();

    build_stream_device();
    init_iic();

    create_kernel_thread(terminal_flush_service, NULL, "TerminalFlush",NULL);
    create_kernel_thread((void *)cpu_speed_test,NULL,"CPUSpeed",NULL);
    pcb_t shell_group = create_process_group("Shell Service");
    create_kernel_thread((void *)shell_setup, NULL, "KernelShell",shell_group);
    kinfo("Kernel load Done!");
    // beep();
    enable_scheduler();

    open_interrupt;

    //page_directory_t *dir = clone_directory(get_kernel_pagedir());


//    for (int i = 0; i < 512; i++) {
//        logkf("page ml4 clone:%p source:%p\n",dir->table->entries[i],get_current_directory()->table->entries[i]);
//    }

    //switch_process_page_directory(dir);
    //cpu_hlt;
    //printk("??\n");

//    page_map_to(dir,0x10000,0x10000,KERNEL_PTE_FLAGS);
//
//    int* a = (int*)0x10000;
//    *a = 1;
//    printk("A: %d\n",*a);

    //    //
    //    vfs_node_t node = vfs_open("/dev/sata0");
    //    if (node != NULL) {
    //        uint8_t *buf = malloc(512);
    //        memset(buf, 0, 512);
    //        vfs_read(node, buf, 0, 512);
    //        for (int i = 0; i < 512; i++) {
    //            logkf("%02x ", buf[i]);
    //        }
    //        logkf("\n");
    //        free(buf);
    //        vfs_close(node);
    //    } else
    //        kerror("Cannot open /dev/sata1");

    cpu_hlt;
}
