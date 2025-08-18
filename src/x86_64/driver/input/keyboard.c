#include "keyboard.h"
#include "atom_queue.h"
#include "heap.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"
#include "terminal.h"

uint8_t keyboard_scancode(uint8_t scancode, uint8_t scancode_1, uint8_t scancode_2);

extern tcb_t        kernel_head_task;
extern lock_queue  *pgb_queue;
struct keyboard_buf kb_fifo;

bool ctrled     = false;
bool shifted    = false;
bool capsLocked = false;

char character_table[140] = {
    0,    27,   '1',  '2', '3', '4', '5', '6', '7',  '8',  '9',  '0',  '-',  '=',  0,    9,
    'q',  'w',  'e',  'r', 't', 'y', 'u', 'i', 'o',  'p',  '[',  ']',  0,    0,    'a',  's',
    'd',  'f',  'g',  'h', 'j', 'k', 'l', ';', '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',', '.', '/', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char shifted_character_table[140] = {
    0,    27,   '!',  '@', '#', '$', '%', '^', '&',  '*',  '(',  ')',  '_',  '+',  0,    '\t',
    'Q',  'W',  'E',  'R', 'T', 'Y', 'U', 'I', 'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H', 'J', 'K', 'L', ':', '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<', '>', '?', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x08, 0x0A, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char cap_character_table[140] = {
    0,    27,   '1',  '2', '3', '4', '5', '6', '7',  '8',  '9',  '0',  '-',  '=',  0,    9,
    'Q',  'W',  'E',  'R', 'T', 'Y', 'U', 'I', 'O',  'P',  '[',  ']',  0,    0,    'A',  'S',
    'D',  'F',  'G',  'H', 'J', 'K', 'L', ';', '\'', '`',  0,    '\\', 'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  ',', '.', '/', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char shifted_cap_character_table[140] = {
    0,    27,   '!',  '@', '#', '$', '%', '^', '&',  '*',  '(',  ')',  '_',  '+',  0,    9,
    'q',  'w',  'e',  'r', 't', 'y', 'u', 'i', 'o',  'p',  '{',  '}',  0,    0,    'a',  's',
    'd',  'f',  'g',  'h', 'j', 'k', 'l', ':', '"',  '~',  0,    '|',  'z',  'x',  'c',  'v',
    'b',  'n',  'm',  '<', '>', '?', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '?', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char *character_array[2][2] = {
    {character_table,     shifted_character_table    },
    {cap_character_table, shifted_cap_character_table}
};

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

    char out = 0;

    if (scancode == 0xE0)
        out = keyboard_scancode(scancode, io_in8(0x60), 0);
    else if (scancode == 0xE1)
        out = keyboard_scancode(scancode, io_in8(0x60), io_in8(0x60));
    else
        out = keyboard_scancode(scancode, 0, 0);

    char *c;
    switch ((uint8_t)out) {
    case CHARACTER_ENTER: c = "\n"; break;
    case CHARACTER_BACK: c = "\b"; break;
    case KEY_BUTTON_UP: c = "\x1b[A"; break;
    case KEY_BUTTON_DOWN: c = "\x1b[B"; break;
    case KEY_BUTTON_LEFT: c = "\x1b[D"; break;
    case KEY_BUTTON_RIGHT: c = "\x1b[C"; break;
    case KEY_BUTTON_HOME: c = "\x1b[H"; break;
    case KEY_BUTTON_PDOWN: c = "\x1b[5~"; break;
    case KEY_BUTTON_END: c = "\x1b[F"; break;
    case KEY_BUTTON_PUP: c = "\x1b[6~"; break;
    case KEY_BUTTON_INSERT: c = "\x1b[2~"; break;
    case KEY_BUTTON_DEL: c = "\x1b[3~"; break;
    default:
        if (ctrled) out &= 0x1f;
        char c0[2];
        c0[0] = out;
        c0[1] = '\0';
        keyboard_tmp_writer((uint8_t *)c0);
        goto end;
    }
    keyboard_tmp_writer((uint8_t *)c);
end:
    send_eoi();
}

uint8_t keyboard_scancode(uint8_t scancode, uint8_t scancode_1, uint8_t scancode_2) {
    if (scancode == 0xE0) {
        switch (scancode_1) {
        case 0x48: return KEY_BUTTON_UP;
        case 0x50: return KEY_BUTTON_DOWN;
        case 0x4b: return KEY_BUTTON_LEFT;
        case 0x4d: return KEY_BUTTON_RIGHT;
        case 0x47: return KEY_BUTTON_HOME;
        case 0x49: return KEY_BUTTON_PDOWN;
        case 0x4f: return KEY_BUTTON_END;
        case 0x51: return KEY_BUTTON_PUP;
        case 0x52: return KEY_BUTTON_INSERT;
        case 0x53: return KEY_BUTTON_DEL;
        default: return 0;
        }
    }
    if (shifted == 1 && scancode & 0x80) {
        if ((scancode & 0x7F) == SCANCODE_SHIFT_L || (scancode & 0x7F) == SCANCODE_SHIFT_R) {
            shifted = 0;
            return 0;
        }
    }

    if (ctrled == 1 && scancode & 0x80) {
        if ((scancode & 0x7F) == 0x1d) {
            ctrled = false;
            return 0;
        }
    }

    if (scancode < sizeof(character_table) && !(scancode & 0x80)) {
        char character = character_array[capsLocked][shifted][scancode];

        if (character != 0) { // Normal char
            return character;
        }

        switch (scancode) {
        case SCANCODE_ENTER: return CHARACTER_ENTER;
        case SCANCODE_BACK: return CHARACTER_BACK;
        case SCANCODE_SHIFT_L:
        case SCANCODE_SHIFT_R: shifted = true; break;
        case 0x1d: ctrled = true; break;
        case SCANCODE_CAPS: capsLocked = !capsLocked; break;
        }
    }

    return 0;
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
    open_interrupt;
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
    close_interrupt;
}
