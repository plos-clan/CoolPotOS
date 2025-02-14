#include "keyboard.h"
#include "isr.h"
#include "krlibc.h"
#include "kprint.h"
#include "io.h"
#include "timer.h"

MouseType type;

__IRQHANDLER void mouse_handle(interrupt_frame_t *frame){
    UNUSED(frame);
    send_eoi();
    uint8_t data = io_in8(PS2_DATA_PORT);
    printk("mouse data: %x\n",data);
}

static inline void wait_for_read(){
    while (io_in8(PS2_CMD_PORT) & KB_STATUS_OBF);
}

static inline void wait_for_write(){
    while (io_in8(PS2_CMD_PORT) & KB_STATUS_IBF);
}

void mouse_setup(){
    return;
    register_interrupt_handler(mouse, mouse_handle,0,0x8E);
    wait_for_write();
    io_out8(PS2_CMD_PORT,KB_EN_MOUSE_INTFACE);
    usleep(100);
    wait_for_write();
    io_out8(PS2_CMD_PORT,KB_SEND2MOUSE);
    wait_for_write();
    io_out8(PS2_CMD_PORT,MOUSE_EN);
    usleep(100);
    wait_for_write();
    io_out8(PS2_CMD_PORT,PS2_DATA_PORT);
    wait_for_write();
    io_out8(PS2_DATA_PORT,KB_INIT_MODE);
    ksuccess("Setup PS/2 mouse.");
}
