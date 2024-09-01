#ifndef CRASHPOWEROS_SYSCALL_H
#define CRASHPOWEROS_SYSCALL_H

#define SYSCALL_PUTC 1
#define SYSCALL_PRINT 2
#define SYSCALL_GETCH 3
#define SYSCALL_MALLOC 4
#define SYSCALL_FREE 5
#define SYSCALL_EXIT 6
#define SYSCALL_G_CLEAN 7
#define SYSCALL_GET_CD 8

#include <stdint.h>
#include <stddef.h>

void syscall_print(char* c);
void syscall_putchar(char c);
char syscall_getch();
void* syscall_malloc(size_t size);
void syscall_free(void *ptr);
void syscall_exit(int code);
void syscall_g_clean();
void syscall_get_cd(char *buffer);

#endif
