#ifndef CRASHPOWEROS_KEYBOARD_H
#define CRASHPOWEROS_KEYBOARD_H

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#include <stdint.h>

typedef struct {
    int is_shift;
    int is_ctrl;
    int is_esc;
}KEY_STATUS;

typedef struct {
    unsigned char *keyboard_map[128];
    unsigned char *shift_keyboard_map[128];
}KEY_MAP;

typedef struct key_listener{
    int lid;
    void (*func)(uint32_t key,int release,char c);
    struct key_listener *next;
}kl_t;

void init_keyboard();
int handle_keyboard_input();
void add_listener(struct key_listener* listener);
void remove_listener(int lid);

#endif //CRASHPOWEROS_KEYBOARD_H
