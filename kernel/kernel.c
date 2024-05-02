#include "../include/multiboot.h"
#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/description_table.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/timer.h"
#include "../include/task.h"
#include "../include/cmos.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/date.h"
#include "../include/acpi.h"
#include "../include/syscall.h"

extern uint32_t end;
extern int status;
uint32_t placement_address = (uint32_t) & end;
multiboot_t *multiboot_all;

void reset_kernel(){
    printf("Restart %s for x86...\n",OS_NAME);
    kill_all_task();
    clock_sleep(10);
    power_reset();
}

void shutdown_kernel(){
    printf("Shutdown %s for x86...\n",OS_NAME);
    kill_all_task();
    clock_sleep(10);
    power_off();
}

uint32_t memory_all(){
    return (multiboot_all->mem_upper + multiboot_all->mem_lower);
}

void kernel_main(multiboot_t *multiboot) {
    multiboot_all = multiboot;
    io_cli();
    vga_install();
    if ((multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1 < 16) {
        printf("[kernel] Minimal RAM amount for CPOS is 16 MB, but you have only %d MB.\n",
               (multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
        while (1) io_hlt();
    }
    initVBE(multiboot);

    printf("[\035kernel\036]: VGA driver load success!\n");
    gdt_install();
    idt_install();
    printf("[\035kernel\036]: description table config success!\n");
    init_timer(10);
    acpi_install();
    printf("[\035kernel\036]: ACPI enable success!\n");
    init_page();
    printf("[\035kernel\036]: page set success!\n");
    init_sched();
    printf("[\035kernel\036]: task load success!\n");
    init_keyboard();
    printf("[\035kernel\036]: Keyboard driver load success!\n");

    syscall_install();
    print_cpu_id();
    io_sti();

    printf("Memory: %dMB.",(multiboot->mem_upper + multiboot->mem_lower)/1024/1024);

    clock_sleep(25);

    kernel_thread(setup_shell, NULL, "CPOS-Shell");
    launch_date();

    for (;;) {
        io_hlt();
        clock_sleep(1);
    }
}