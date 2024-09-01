#ifndef CRASHPOWEROS_KEYBOARD_H
#define CRASHPOWEROS_KEYBOARD_H

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#include <stdint.h>

void init_keyboard();
int kernel_getch();

#endif //CRASHPOWEROS_KEYBOARD_H
