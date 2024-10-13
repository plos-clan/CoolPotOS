#ifndef CRASHPOWEROS_BOOT_ARG_H
#define CRASHPOWEROS_BOOT_ARG_H

#define TTY_DEFAULT 2
#define TTY_OS_TERMINAL 4
#define TTY_UNKNOWN 8
#define TTY_IS_OPEN 16

void boot_arg(char* args);

#endif