#include "keyboard.h"
#include "heap.h"
#include "io.h"
#include "ipc.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"

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
}key_cmd_state = {
    .waiting_ack  = false,
    .got_ack      = false,
    .got_resend   = false,
};

static void key_callback(void *pcb_handle, void *scan_handle) {
    pcb_t pcb = (pcb_t)pcb_handle;
    if (pcb->status == DEATH) return;
    uint8_t       scancode = *((uint8_t *)scan_handle);
    ipc_message_t message  = (ipc_message_t)malloc(sizeof(struct ipc_message));
    message->type          = IPC_MSG_TYPE_KEYBOARD;
    message->pid           = pcb->pgb_id;
    message->data[0]       = scancode;
    ipc_send(pcb, message);
}

__IRQHANDLER void keyboard_handler(interrupt_frame_t *frame) {
    io_out8(0x61, 0x20);
    uint8_t scancode = io_in8(0x60);
    send_eoi();

    logkf("key: %x\n", scancode);

    if(scancode == 0xfa) {
        key_cmd_state.got_ack = true;
        return;
    }

    if(scancode == 0xaa && key_cmd_state.got_ack && !key_cmd_state.got_resend) {
        key_cmd_state.got_resend = true;
        return;
    }

    if (scancode == 0x2a || scancode == 0x36) { // Shift按下
        shift = 1;
    }
    if (scancode == 0x1d) { // Ctrl按下
        ctrl = 1;
    }
    if (scancode == 0x3a) { // Caps Lock按下
        caps_lock = caps_lock ^ 1;
    }
    if (scancode == 0xaa || scancode == 0xb6) { // Shift松开
        shift = 0;
    }
    if (scancode == 0x9d) { // Ctrl按下
        ctrl = 0;
    }
    logkf("key: %x\n", scancode);
    if (scancode < 0x80 || scancode == 0xe0) {
        if(get_current_task() == NULL) return;
        if (pgb_queue) queue_iterate(pgb_queue, key_callback, &scancode);
    }
    logkf("keyI: %x\n", scancode);
}

int input_char_inSM() {
    int   i    = -1;
    tcb_t task = get_current_task();
    if (task == NULL) return 0;
    task->status                         = WAIT;
    task->parent_group->tty->is_key_wait = true;
    ipc_message_t message                = ipc_recv_wait(IPC_MSG_TYPE_KEYBOARD);
    i                                    = message->data[0];
    free(message);
    logkf("IPC RECV: %d\n", i);
    task->parent_group->tty->is_key_wait = false;
    task->status                         = RUNNING;
    return i;
}

int kernel_getch() {
    tcb_t task = get_current_task();
    logkf("getch task: %d %d\n", task->seq_state, task->last_key);

    if (task->seq_state == 1) {
        task->seq_state = 2;
        return '[';
    }
    if (task->seq_state == 2) {
        task->seq_state = 0;
        switch (task->last_key) {
        case 1: return 'A';
        case 2: return 'B';
        case 3: return 'D';
        case 4: return 'C';
        default: return '?';
        }
    }

    uint8_t ch;
    do {
        ch = input_char_inSM(); // 扫描码
    } while (ch == 0xff);
    if (ch == 0xe0) { // keytable之外的键（↑,↓,←,→）
        ch              = input_char_inSM();
        task->seq_state = 1;
        switch (ch) {
        case 0x48: task->last_key = 1; return 0x1B; // ↑
        case 0x50: task->last_key = 2; return 0x1B; // ↓
        case 0x4b: task->last_key = 3; return 0x1B; // ←
        case 0x4d: task->last_key = 4; return 0x1B; // →
        default: return 0;
        }
    }
    // 返回扫描码(keytable之内)对应的ASCII码
    if (keytable[ch] == 0x00) {
        return 0;
    } else if (shift == 0 && caps_lock == 0) {
        return keytable1[ch];
    } else if (shift == 1 || caps_lock == 1) {
        return keytable[ch];
    }
    return -1;
}

static inline uint8_t ps2_status() {
    return io_in8(PS2_CMD_PORT);
}

static bool ps2_wait_input_clear() {
    for (size_t i = 0; i < MAX_WAIT_INDEX; i++) {
        if(!(ps2_status() & 0x02)) {
            return true;
        }
    }
    return false;
}

bool ps2_send_cmd(uint8_t cmd) {
    if(!ps2_wait_input_clear()) return false;
    io_out8(PS2_DATA_PORT, cmd);
    return true;
}

bool ps2_keyboard_exists() {
    if(!ps2_send_cmd(0xFF)) return false;
    return true;
}

void keyboard_setup() {
    register_interrupt_handler(keyboard, (void *)keyboard_handler, 0, 0x8E);
    if(ps2_keyboard_exists()){
        kinfo("Setup PS/2 keyboard.");
    }
}
