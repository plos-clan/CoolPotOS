#include "acpi.h"
#include "ahci.h"
#include "cpuid.h"
#include "cpusp.h"
#include "description_table.h"
#include "devfs.h"
#include "dlinker.h"
#include "frame.h"
#include "gop.h"
#include "heap.h"
#include "hhdm.h"
#include "ide.h"
#include "iic/iic_core.h"
#include "keyboard.h"
#include "killer.h"
#include "kprint.h"
#include "krlibc.h"
#include "limine.h"
#include "module.h"
#include "page.h"
#include "pcb.h"
#include "pcie.h"
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
#include "vdisk.h"
#include "vfs.h"
#include "xhci.h"
#include "modfs.h"

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

/*
void sb16_test_sound() {
    sb16_set_sample_rate(115200);
    sb16_set_volume(15, 15);
    sb16_play(音频数据, 数据大小);
}*/

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
    init_tty();
    printk("CoolPotOS %s (git:%s) (%s version %s) on an x86_64\n", KERNEL_NAME, GIT_VERSION,
           COMPILER_NAME, COMPILER_VERSION);
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);
    error_setup();
    float_processor_setup();
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
    iso9660_regist();

    vdisk_init();
    devfs_setup();
    modfs_setup();
    pcie_init();
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

    build_stream_device();

    killer_setup();
    setup_syscall();
    xhci_setup();
    kinfo("Kernel load Done!");

    create_kernel_thread(terminal_flush_service, NULL, "TerminalFlush", NULL);

    pcb_t shell_group = create_process_group("Shell Service", NULL, NULL);
    create_kernel_thread((void *)shell_setup, NULL, "KernelShell", shell_group);
    create_kernel_thread((void *)cpu_speed_test, NULL, "CPUSpeed", NULL);

    // beep();
    open_interrupt;
    enable_scheduler();

//    vfs_node_t device = vfs_open("/dev/sata0");
//    if (device != NULL) {
//        uint8_t *data = malloc(512);
//        int      a    = vfs_read(device, data, 0, 512);
//        if (a == VFS_STATUS_SUCCESS) {
//            kinfo("Read data from /dev/sata0");
//            for (int i = 0; i < 512; i++) {
//                logkf("%02x ", data[i]);
//            }
//            logkf("\n");
//        } else {
//            kerror("Failed to read from /dev/sata0 %d\n", a);
//        }
//        vfs_close(device);
//    } else {
//        kerror("Cannot open /dev/sata0");
//    }

    halt_service();
}
