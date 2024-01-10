#include "../include/console.h"
#include "../include/redpane.h"
#include "../include/common.h"
#include "../include/memory.h"
#include "../include/gdt.h"
#include "../include/io.h"
#include "../include/idt.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/kheap.h"
#include "../include/shell.h"
#include "../include/stack.h"
#include "../include/acpi.h"
#include "../include/thread.h"
#include "../include/task.h"
#include "../include/multiboot.h"

extern uint32_t end; // declared in linker.ld
extern ThreadManager *manager; //Thread Manager
uint32_t placement_address = (uint32_t) & end;

void kernel_main(struct multiboot *mboot_ptr, u32int initial_stack) {

    gdt_install();
    idt_install();
    error_setup();
    terminal_initialize();
    acpi_install();

    init_page();

    register_interrupt_handler(0x21, handle_keyboard_input);
    asm("sti");

    init_timer(50);
    init_keyboard();

    if (!is_protected_mode_enabled()) {
        show_VGA_red_pane("PROTECTED_MODE_ERROR", "The current OS is not running is 32-bit protected mode.", 0x01,
                          NULL);
    }
    terminal_clear();
    printf("BOOT COUNT [%s]",mboot_ptr->boot_loader_name);
    task_setup();
}