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

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2)

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

extern void error_setup(); //error_handle.c

int func(void *pVoid) {
    UNUSED(pVoid);
    while (1){
        printk("Hello World!\n");
        usleep(1000);
    }
    return 0;
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

   // create_kernel_thread(func, NULL, "Test");
    kinfo("Kernel load Done!");
    beep();

    open_interrupt;

    cpu_hlt;
}
