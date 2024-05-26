#ifndef CRASHPOWEROS_SERIAL_H
#define CRASHPOWEROS_SERIAL_H

#define SERIAL_PORT 0x3f8

int init_serial();
int serial_received();
char read_serial();
int is_transmit_empty();
void write_serial(char a);

#endif
