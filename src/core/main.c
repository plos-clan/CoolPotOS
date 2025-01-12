#include "limine.h"
#include "gop.h"
#include "kprint.h"
#include "terminal.h"
#include "hhdm.h"
#include "frame.h"
#include "heap.h"
#include "krlibc.h"
#include "acpi.h"
#include "klog.h"
#include "description_table.h"
#include "page.h"
#include "cpuid.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2)

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

extern void error_setup(); //error_handle.c

void kmain(void) {
    init_gop();
    init_serial();
    init_hhdm();
    init_frame();
    init_heap();
    init_terminal();

    printk("CoolPotOS %s (Limine Bootloader) on an x86_64\n", KERNEL_NAME);
    init_cpuid();
    kinfo("Video: 0x%p - %d x %d", framebuffer->address, framebuffer->width, framebuffer->height);

    gdt_setup();
    idt_setup();
    error_setup();
    page_setup();


    acpi_setup();

    kinfo("Kernel load Done!");
    open_interrupt;

    cpu_hlt;
}
