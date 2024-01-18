#include "../include/vga.h"
#include "../include/io.h"
#include "../include/description_table.h"
#include "../include/multiboot.h"
#include "../include/memory.h"
#include "../include/shell.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/task.h"

extern uint32_t end;
uint32_t placement_address = (uint32_t) & end;

void kernel_main(unsigned long magic,struct multiboot_info *mbi){

    vga_install();
    install_idt();
    install_gdt();

    init_page();
    init_keyboard();

    init_timer(50);
    init_task();

    asm("sti");

    setup_shell();
}