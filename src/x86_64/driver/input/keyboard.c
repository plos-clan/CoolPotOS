#include "keyboard.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "os_terminal.h"
#include "pcb.h"
#include "smp.h"

static int   caps_lock, shift, ctrl = 0;
spin_t       keyboard_lock;
extern tcb_t kernel_head_task;

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

static void key_callback(void *pcb_handle, void *scan_handle) {
    tcb_t   cur      = (tcb_t)pcb_handle;
    uint8_t scancode = *((uint8_t *)scan_handle);
    if (cur->status == DEATH) return;
    queue_enqueue(cur->parent_group->tty->keyboard_buffer, (void *)scancode);
}

__IRQHANDLER void keyboard_handler(interrupt_frame_t *frame) {
    spin_lock(keyboard_lock);
    io_out8(0x61, 0x20);
    uint8_t scancode = io_in8(0x60);
    send_eoi();
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

    if (scancode < 0x80 || scancode == 0xe0) {
        for (size_t i = 0; i < MAX_CPU; i++) {
            smp_cpu_t cpu = smp_cpus[i];
            if (cpu.ready == 1) { queue_iterate(cpu.scheduler_queue, key_callback, &scancode); }
        }
    }
    spin_unlock(keyboard_lock);
}

int input_char_inSM() {
    int   i    = -1;
    tcb_t task = get_current_task();
    if (task == NULL) return 0;
    task->status                         = WAIT;
    task->parent_group->tty->is_key_wait = true;
    do {
        i = (int)queue_dequeue(task->parent_group->tty->keyboard_buffer);
        __asm__ volatile("pause");
    } while (i == -1);
    task->parent_group->tty->is_key_wait = false;
    task->status                         = RUNNING;

    // terminal_handle_keyboard(i);
    // return -1;

    return i;
}

int kernel_getch() {
    uint8_t ch;
    do {
        ch = input_char_inSM(); // 扫描码
    } while (ch == 0xff);
    if (ch == 0xe0) { // keytable之外的键（↑,↓,←,→）
        ch = input_char_inSM();
        if (ch == 0x48) { // ↑
            return -2;
        } else if (ch == 0x50) { // ↓
            return -3;
        } else if (ch == 0x4b) { // ←
            return -4;
        } else if (ch == 0x4d) { // →
            return -5;
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

void keyboard_setup() {
    register_interrupt_handler(keyboard, (void *)keyboard_handler, 0, 0x8E);
    kinfo("Setup PS/2 keyboard.");
}
