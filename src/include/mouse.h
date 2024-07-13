#ifndef CRASHPOWEROS_MOUSE_H
#define CRASHPOWEROS_MOUSE_H

#include "common.h"

uint8_t mouse_read();
void mouse_reset();
void mouse_wait(uint8_t a_type);
void mouse_write(uint8_t a_write);
void mouse_install();

#endif
