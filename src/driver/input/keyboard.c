#include "isr.h"
#include "description_table.h"
#include "krlibc.h"
#include "io.h"
#include "kprint.h"
#include "acpi.h"

__IRQHANDLER void keyboard_handler(interrupt_frame_t *frame){
    UNUSED(frame);
    uint8_t scancode = io_in8(0x60);
    printk("Keyboard scancode: %x\n", scancode);
    send_eoi();
}

void keyboard_setup(){
    register_interrupt_handler(keyboard, (void *) keyboard_handler, 0, 0x8E);
    kinfo("Setup PS/2 keyboard.");
}
