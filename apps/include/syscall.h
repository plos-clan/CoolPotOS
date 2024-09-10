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

#include "ctype.h"

void syscall_print(char *c);

void syscall_putchar(char c);

char syscall_getch();

void *syscall_malloc(size_t size);

void syscall_free(void *ptr);

void syscall_exit(int code);

void syscall_g_clean();

void syscall_get_cd(char *buffer);

int syscall_vfs_filesize(char *filename);

void syscall_vfs_readfile(char *filename, char *buffer);

void syscall_vfs_writefile(char *filename, char *buffer, unsigned int size);

void *syscall_sysinfo();

int syscall_exec(char *filename, char *args, int is_async);

void syscall_vfs_change_path(const char *path);

char *syscall_get_arg();

long syscall_clock();

void syscall_sleep(uint32_t timer);

int syscall_vfs_remove_file(char *filename);

int syscall_vfs_rename(char *filename1, char *filename2);

#endif
