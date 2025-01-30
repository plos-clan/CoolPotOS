#include "limine.h"
#include "gop.h"
#include "kprint.h"
#include "terminal.h"
#include "hhdm.h"
#include "frame.h"
#include "heap.h"
#include "krlibc.h"
#include "acpi.h"
#include "timer.h"
#include "power.h"
#include "description_table.h"
#include "page.h"
#include "keyboard.h"
#include "cpuid.h"
#include "pcb.h"
#include "pci.h"
#include "ahci.h"
#include "speaker.h"
#include "shell.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2)

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

extern void error_setup(); //error_handle.c

int terminal_flush_service(void *pVoid) {
    terminal_set_auto_flush(0);
    while (1){
        terminal_flush();
        __asm__("hlt":::"memory");
    }
    return 0;
}

_Noreturn void cp_shutdown(){
    printk("Shutdown %s...\n", KERNEL_NAME);
   // kill_all_proc();
    power_off();
    while (1);
}

_Noreturn void cp_reset(){
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
    page_setup();
    init_heap();
    init_terminal();
    init_tty();
    printk("CoolPotOS %s (Limine Bootloader) on an x86_64\n", KERNEL_NAME);
    init_cpuid();
    printk("Video: 0x%p - %d x %d\n", framebuffer->address, framebuffer->width, framebuffer->height);

    gdt_setup();
    idt_setup();
    error_setup();

    acpi_setup();
    keyboard_setup();
    mouse_setup();
    char* date = get_date_time();
    kinfo("RTC time %s",date);
    free(date);

    pci_setup();
    ahci_setup();

    init_pcb();

    create_kernel_thread(terminal_flush_service, NULL, "Terminal");
    create_kernel_thread((void*)shell_setup, NULL, "Shell");
    kinfo("Kernel load Done!");
    beep();
    enable_scheduler();
    open_interrupt;

    cpu_hlt;
}
