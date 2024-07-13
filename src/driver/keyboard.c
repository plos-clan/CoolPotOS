#include "../include/keyboard.h"
#include "../include/graphics.h"
#include "../include/memory.h"
#include "../include/queue.h"
#include "../include/io.h"

KEY_STATUS *key_status;
Queue *key_char_queue;
struct key_listener* head_listener;

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

static void default_handle(uint32_t key,int release,char c){
    if(!release) {
        if(key == 42) {
            key_status->is_shift = 1;
            return 0;
        }

        if(key == 29){
            key_status->is_ctrl = 1;
            return 0;
        }

        if(c == 0) return 0;

        if(key == 0x81) queue_push(key_char_queue,-5);

        queue_push(key_char_queue,(char)c);
    } else {
        if(key == -86){
            key_status->is_shift = 0;
        }

        if(key == 29){
            key_status->is_ctrl = 0;
            return 0;
        }
    }
}

void init_keyboard(){
    key_status = (KEY_STATUS*) alloc(sizeof(KEY_STATUS));
    if(key_status == NULL) goto error;
    key_status->is_shift = 0;

    key_char_queue = create_queue();
    head_listener = (struct key_listener*) kmalloc(sizeof(struct key_listener));
    if(head_listener == NULL) goto error;
    head_listener->func = default_handle;
    head_listener->lid = 0;
    head_listener->next = NULL;

    register_interrupt_handler(0x21,handle_keyboard_input);

    klogf(true,"Load PS/2 Keyboard device.\n");
    return;
    error:
    klogf(false,"Load PS/2 Keyboard device.\n");
}

int handle_keyboard_input(registers_t *reg){
    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    uint32_t key = read_port(KEYBOARD_DATA_PORT);
    int release = key & 0xb10000000;
    char c = key_status->is_shift ? shift_keyboard_map[(unsigned char )key] : keyboard_map[(unsigned char )key];

    struct key_listener* h = head_listener;
    while (1){
        h->func(key,release,c);
        h = h->next;
        if(h == NULL) break;
    }

    return 0;
}

static void found_listener(int lid,struct key_listener *head,struct key_listener *base,struct key_listener **argv,int first){
    struct key_listener *t = base;
    if(t == NULL){
        argv = NULL;
        return;
    }
    if(t->lid == lid){
        *argv = t;
        return;
    } else{
        if(!first)
            if(head->lid == t->lid){
                argv = NULL;
                return;
            }
        found_listener(lid,head,t->next,argv,0);
    }
}

struct key_listener* found_listener_id(int lid){
    struct task_struct *argv = NULL;
    found_listener(lid,head_listener,head_listener,&argv,1);
    if(argv == NULL){
        printf("Cannot found key listener id:[%d].\n",lid);
        return NULL;
    }
    return argv;
}

void remove_listener(int lid){
    struct key_listener *argv = found_listener_id(lid);
    if(argv == NULL){
        printf("Cannot found listener id:[%d].\n",lid);
        return;
    }
    if(argv->lid == 0){
        printf("[\033driver\036]: Cannot remove default listener.\n");
        return;
    }

    kfree(argv);
    struct key_listener *head = head_listener;
    struct key_listener *last = NULL;
    while (1){
        if(head->lid == argv->lid){
            last->next = argv->next;
            return;
        }
        last = head;
        head = head->next;
    }
}

void add_listener(struct key_listener* listener){
    if(listener == NULL) return;

    struct key_listener* h = head_listener,*buf = NULL;
    while (1){
        buf = h;
        h = h->next;
        if(h == NULL){
            buf->next = listener;
            break;
        }
    }
}