#ifndef CRASHPOWEROS_SYSCALL_H
#define CRASHPOWEROS_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#define MAX_SYSCALLS 256

#define SYSCALL_PUTC 1
#define SYSCALL_PRINT 2
#define SYSCALL_GETCH 3
#define SYSCALL_MALLOC 4
#define SYSCALL_FREE 5
#define SYSCALL_EXIT 6
#define SYSCALL_G_CLEAN 7
#define SYSCALL_GET_CD 8
#define SYSCALL_VFS_FILESIZE 9
#define SYSCALL_VFS_READFILE 10
#define SYSCALL_VFS_WRITEFILE 11
#define SYSCALL_SYSINFO 12
#define SYSCALL_EXEC 13
#define SYSCALL_CHANGE_PATH 14
#define SYSCALL_GET_ARG 15
#define SYSCALL_CLOCK 16
#define SYSCALL_SLEEP 17
#define SYSCALL_VFS_REMOVE_FILE 18
#define SYSCALL_VFS_RENAME 19
#define SYSCALL_ALLOC_PAGE 20
#define SYSCALL_FRAMEBUFFER 21
#define SYSCALL_DRAW_BITMAP 22

void syscall_install();
uint32_t syscall_exec(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi);

#endif
