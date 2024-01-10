#include "../include/keyboard.h"
#include "../include/console.h"
#include "../include/kheap.h"
#include "../include/queue.h"
#include "../include/io.h"

static KEY_STATUS *key_status;
Queue *key_char_queue;


unsigned char keyboard_map[128] =
        {
                0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
                '9', '0', '-', '=', '\b',	/* Backspace */
                '\t',			/* Tab */
                'q', 'w', 'e', 'r',	/* 19 */
                't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
                0,			/* 29   - Control */
                'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
                '\'', '`',   -1,		/* Left shift */
                '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
                'm', ',', '.', '/',   -1,				/* Right shift */
                '*',
                0,	/* Alt */
                ' ',	/* Space bar */
                0,	/* Caps lock */
                0,	/* 59 - F1 key ... > */
                0,   0,   0,   0,   0,   0,   0,   0,
                0,	/* < ... F10 */
                0,	/* 69 - Num lock*/
                0,	/* Scroll Lock */
                0,	/* Home key */
                0,	/* Up Arrow */
                0,	/* Page Up */
                '-',
                0,	/* Left Arrow */
                0,
                0,	/* Right Arrow */
                '+',
                0,	/* 79 - End key*/
                0,	/* Down Arrow */
                0,	/* Page Down */
                0,	/* Insert Key */
                0,	/* Delete Key */
                0,   0,   0,
                0,	/* F11 Key */
                0,	/* F12 Key */
                0,	/* All other keys are undefined */
        };

unsigned char shift_keyboard_map[128] =
        {
                0,  27, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
                '(', ')', '_', '+', '\b',	/* Backspace */
                '\t',			/* Tab */
                'Q', 'W', 'E', 'R',	/* 19 */
                'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
                0,			/* 29   - Control */
                'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
                '"', '~',   -1,		/* Left shift */
                '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
                'M', '<', '>', '?',   -1,				/* Right shift */
                '*',
                0,	/* Alt */
                ' ',	/* Space bar */
                0,	/* Caps lock */
                0,	/* 59 - F1 key ... > */
                0,   0,   0,   0,   0,   0,   0,   0,
                0,	/* < ... F10 */
                0,	/* 69 - Num lock*/
                0,	/* Scroll Lock */
                0,	/* Home key */
                0,	/* Up Arrow */
                0,	/* Page Up */
                '-',
                0,	/* Left Arrow */
                0,
                0,	/* Right Arrow */
                '+',
                0,	/* 79 - End key*/
                0,	/* Down Arrow */
                0,	/* Page Down */
                0,	/* Insert Key */
                0,	/* Delete Key */
                0,   0,   0,
                0,	/* F11 Key */
                0,	/* F12 Key */
                0,	/* All other keys are undefined */
        };

void init_keyboard(){
    key_status = (KEY_STATUS*) alloc(sizeof(KEY_STATUS));
    key_status->is_shift = 0;

    key_char_queue = create_queue();
}

int handle_keyboard_input(){

    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    uint32_t key = read_port(KEYBOARD_DATA_PORT);
    int release = key & 0xb10000000;
    char c = key_status->is_shift ? shift_keyboard_map[(unsigned char )key] : keyboard_map[(unsigned char )key];

    if(!release) {
        if(c == -1) {
            key_status->is_shift = 1;
            return 0;
        }

        queue_push(key_char_queue,(char)c);
    } else {
        if(c == -1){
            key_status->is_shift = 0;
        }
    }

    return 0;
}