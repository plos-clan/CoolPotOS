#include "keyboard.h"
#include "isr.h"
#include "krlibc.h"
#include "kprint.h"

__IRQHANDLER void mouse_handle(interrupt_frame_t *frame){
    UNUSED(frame);
    send_eoi();
    printk("m");
}

void mouse_setup(){
    register_interrupt_handler(mouse, mouse_handle,0,0x8E);
    kinfo("Setup PS/2 mouse.");
}
