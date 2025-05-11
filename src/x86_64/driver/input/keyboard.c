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

static inline uint8_t ps2_status() {
    return io_in8(PS2_CMD_PORT);
}

static bool ps2_wait_input_clear() {
    for (size_t i = 0; i < MAX_WAIT_INDEX; i++) {
        if (!(ps2_status() & 0x02)) { return true; }
    }
    return false;
}

bool ps2_send_cmd(uint8_t cmd) {
    if (!ps2_wait_input_clear()) return false;
    io_out8(PS2_DATA_PORT, cmd);
    return true;
}

bool ps2_keyboard_exists() {
    if (!ps2_send_cmd(0xFF)) return false;
    return true;
}

void keyboard_setup() {
    register_interrupt_handler(keyboard, (void *)keyboard_handler, 0, 0x8E);
    if (ps2_keyboard_exists()) { kinfo("Setup PS/2 keyboard."); }
}
