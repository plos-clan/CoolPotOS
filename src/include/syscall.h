#ifndef CRASHPOWEROS_SYSCALL_H
#define CRASHPOWEROS_SYSCALL_H

#include <stddef.h>

#define MAX_SYSCALLS 256

#define SYSCALL_PUTC 1
#define SYSCALL_PRINT 2
#define SYSCALL_GETC 3

void syscall_install();

#endif
