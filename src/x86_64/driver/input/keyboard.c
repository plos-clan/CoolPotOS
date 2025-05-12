#include "keyboard.h"
#include "atom_queue.h"
#include "heap.h"
#include "io.h"
#include "ipc.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"
#include "terminal.h"

static int         caps_lock, shift, ctrl = 0;
extern tcb_t       kernel_head_task;
extern lock_queue *pgb_queue;

struct keyboard_buf kb_fifo;

char keytable[0x54] = { // 按下Shift
    0,   0x01, '!', '@', '#', '$', '%',  '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q',
    'W', 'E',  'R', 'T', 'Y', 'U', 'I',  'O', 'P', '{', '}', 10,  0,   'A', 'S',  'D',  'F',
    'G', 'H',  'J', 'K', 'L', ':', '\"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B',  'N',  'M',
    '<', '>',  '?', 0,   '*', 0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,    0,    0,
    0,   0,    0,   '7', 'D', '8', '-',  '4', '5', '6', '+', '1', '2', '3', '0',  '.'};

char keytable1[0x54] = { // 未按下Shift
    0,   0x01, '1', '2', '3', '4', '5',  '6', '7', '8',  '9', '0', '-', '=', '\b', '\t', 'q',
    'w', 'e',  'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', 10,  0,   'a', 's',  'd',  'f',
    'g', 'h',  'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v', 'b',  'n',  'm',
    ',', '.',  '/', 0,   '*', 0,   ' ',  0,   0,   0,    0,   0,   0,   0,   0,    0,    0,
    0,   0,    0,   '7', '8', '9', '-',  '4', '5', '6',  '+', '1', '2', '3', '0',  '.'};

struct keyboard_cmd_state {
    bool waiting_ack;
    bool got_ack;
    bool got_resend;
} key_cmd_state = {
    .waiting_ack = false,
    .got_ack     = false,
    .got_resend  = false,
};

__IRQHANDLER void keyboard_handler(interrupt_frame_t *frame) {
    uint8_t scancode = io_in8(0x60);

    if (scancode == 0xfa) {
        key_cmd_state.got_ack = true;
        send_eoi();
        return;
    }

    if (scancode == 0xaa && key_cmd_state.got_ack && !key_cmd_state.got_resend) {
        key_cmd_state.got_resend = true;
        send_eoi();
        return;
    }

    /* temp solution until can pass all scancode to threads */
    terminal_handle_keyboard(scancode);
    send_eoi();
}

int kernel_getch() {
    char               ch;
    extern atom_queue *temp_keyboard_buffer;
    while ((ch = atom_pop(temp_keyboard_buffer)) == -1) {
        __asm__ volatile("pause");
    }
    return ch;
}

void wait_ps2_write() {
    for (size_t i = 0; i < MAX_WAIT_INDEX; ++i) {
        if (!(io_in8(PS2_CMD_PORT) & KB_STATUS_IBF)) return;
    }
}

void wait_ps2_read() {
    for (size_t i = 0; i < MAX_WAIT_INDEX; ++i) {
        if (!(io_in8(PS2_CMD_PORT) & KB_STATUS_OBF)) return;
    }
}

void keyboard_setup() {
    kb_fifo.p_head = kb_fifo.buf;
    kb_fifo.p_tail = kb_fifo.buf;
    kb_fifo.count  = 0;
    register_interrupt_handler(keyboard, (void *)keyboard_handler, 0, 0x8E);
    wait_ps2_write();
    io_out8(PS2_CMD_PORT, 0xAD);
    while (io_in8(PS2_CMD_PORT) & KB_STATUS_OBF) {
        io_in8(PS2_DATA_PORT);
    }
    wait_ps2_write();
    io_out8(PS2_CMD_PORT, KBCMD_WRITE_CMD); // 0x60
    wait_ps2_write();
    io_out8(PS2_DATA_PORT, KB_INIT_MODE); // 0x03

    wait_ps2_write();
    io_out8(PS2_CMD_PORT, 0xAE);

    wait_ps2_write();
    io_out8(PS2_DATA_PORT, 0xFF); // 重置命令
    wait_ps2_read();
    uint8_t response = io_in8(PS2_DATA_PORT);
    if (response != 0xAA) { kerror("Keyboard reset failed: 0x%x", response); }

    wait_ps2_write();
    io_out8(PS2_DATA_PORT, 0xF4); // 启用数据报告
    wait_ps2_read();
    if (io_in8(PS2_DATA_PORT) != 0xFA) { kerror("Keyboard enable failed"); }
    kinfo("Setup PS/2 keyboard.");
}
