#ifndef CRASHPOWEROS_SYSCALL_H
#define CRASHPOWEROS_SYSCALL_H

#include <stddef.h>

#define MAX_SYSCALLS 256

#define SYSCALL_PUTC 1
#define SYSCALL_PRINT 2
#define SYSCALL_GETCH 3
#define SYSCALL_MALLOC 4
#define SYSCALL_FREE 5
#define SYSCALL_EXIT 6
#define SYSCALL_G_CLEAN 7
#define SYSCALL_GET_CD 8

void syscall_install();

#endif
