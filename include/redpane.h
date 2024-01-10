#ifndef CPOS_REDPANE_H
#define CPOS_REDPANE_H

#include <stdint.h>
#include "isr.h"

void show_OK_blue_pane(char *type,char *message);
void show_VGA_red_pane(char* type,char* message,uint32_t hex,registers_t *page);

#endif
