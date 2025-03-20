#include "video.h"
#include "multiboot.h"
#include "description_table.h"
#include "io.h"
#include "tty.h"
#include "klog.h"
#include "timer.h"
#include "acpi.h"
#include "page.h"
#include "free_page.h"
#include "error.h"
#include "pci.h"
#include "ide.h"
#include "ahci.h"
#include "vdisk.h"
#include "vfs.h"
#include "devfs.h"
#include "net.h"
#include "os_terminal.h"
#include "pcb.h"
#include "cpuid.h"
#include "keyboard.h"
#include "mouse.h"
#include "scheduler.h"
#include "krlibc.h"
#include "syscall.h"
#include "speaker.h"
#include "fpu.h"
#include "hda.h"
#include "vsound.h"
#include "shell.h"
#include "iic_core.h"
#include "os_terminal.h"

extern void *program_break_end;
extern void *program_break;

extern void iso9660_regist(); //iso9660.c
extern void fatfs_regist(); //fat.c
extern void pipfs_regist(); //pipfs.c

_Noreturn void shutdown() {
    printk("Shutdown %s...\n", KERNEL_NAME);
    kill_all_proc();
    sleep(10);
    power_off();
    while (1);
}

_Noreturn void reboot() {
    printk("Shutdown %s...\n", KERNEL_NAME);
    kill_all_proc();
    sleep(10);
    power_reset();
    while (1);
}

int terminal_manual_flush(void *arg) {
    terminal_set_auto_flush(0);
    while (1) {
        terminal_flush();
        io_hlt();
    }
}

/*
 * 内核初始化函数, 最终会演变为CPU0的IDLE进程
 * > 注意, 其所有的函数调用顺序均不可改变. 需按顺序初始化OS功能
 * @Noreturn
 */
_Noreturn void kernel_main(multiboot_t *multiboot, uint32_t kernel_stack) {
    //内核堆初始化
    program_break_end = program_break + 0x300000 + 1 + KHEAP_INITIAL_SIZE;
    memset(program_break, 0, program_break_end - program_break);

    vga_install();
    gdt_install();
    idt_install(); //8259A PIC初始化
    tty_init(); //tty 设备初始化
    init_vbe(multiboot);

    default_terminal_setup();
    extern void check_memory(multiboot_t *multiboot);
    check_memory(multiboot);

//    if (multiboot->flags & (1 << 2)) {
//
//    }
    disable_scheduler();
    page_init(multiboot); //分页开启
    setup_free_page();
    terminal_setup(false);
    printk("CoolPotOS %s (Limine Multiboot) on an i386\n", KERNEL_NAME);
    printk("KernelArea: 0x00000000 - 0x%08x | GraphicsBuffer: 0x%08x \n",
           program_break_end,
           multiboot->framebuffer_addr);
    init_cpuid();
    klogf(true, "Memory manager initialize.\n");
    setup_error();
    init_fpu(); //初始化浮点处理器
    acpi_install();  //ACPI初始化
    init_timer(1); //RTC 时钟中断
    vdisk_init();
    vfs_init();
    init_pci(); //pci设备列表加载, 所有PCI设备相关驱动初始化需在此函数后方调用

    ide_init();
    ahci_init();
    //hda_init();
    //hda_regist();
    iic_init();

    devfs_regist();

    io_cli(); //ide等块设备驱动会打开中断以加载硬盘设备, 需重新关闭中断以继续初始化其余OS功能
    iso9660_regist();
    fatfs_regist();
    init_pcb();
    pipfs_regist();
    //net_setup();
    keyboard_init();
    mouse_init();

    setup_syscall();

    create_kernel_thread(terminal_manual_flush, NULL, "Terminal");

    create_kernel_thread((void *) setup_shell, NULL, "Shell");
    klogf(true, "Enable kernel shell service.\n");

    // 挂载最后一个块设备(通常为引导设备)
    vfs_node_t dev = vfs_open("/dev");
    vfs_node_t c = NULL;
    bool win = false;
    list_foreach(dev->child, i) {
        c = (vfs_node_t) i->data;
        char buf[20];
        sprintf(buf, "/dev/%s", c->name);
        if (vfs_mount(buf, vfs_open("/")) != -1) {
            win = true;
            break;
        }
    }
    if (c == NULL || !win) {
        klogf(false, "Cannot mount a drive device.\n");
    } else {
        klogf(true, "Block device mount success!\n");
    }

    klogf(true, "Kernel load done!\n");
    beep();
    enable_scheduler();
    io_sti(); //内核加载完毕, 打开中断以启动进程调度器, 开始运行

    while (1) {
        free_pages();
        //io_hlt();
    }
}