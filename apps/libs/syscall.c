#include "../include/syscall.h"
#include "../include/stdio.h"
#include "syscall.h"

typedef int ssize_t;

void syscall_print(char* c) {
    __syscall(SYSCALL_PRINT, c);
}

void syscall_putchar(char c) {
    __syscall(SYSCALL_PUTC, c);
}

int syscall_getch(){
    return __syscall(SYSCALL_GETCH);
}

void* syscall_malloc(size_t size){
    return (void *)__syscall(SYSCALL_MALLOC, size);
}

void syscall_free(void *ptr) {
    __syscall(SYSCALL_FREE, ptr);
}

void syscall_exit(int code) {
    __syscall(SYSCALL_EXIT, code);
}

void syscall_g_clean(){
    __syscall(SYSCALL_G_CLEAN);
}

void syscall_get_cd(char *buffer) {
    __syscall(SYSCALL_GET_CD, buffer);
}

int syscall_vfs_filesize(char* filename) {
    return __syscall(SYSCALL_VFS_FILESIZE, filename);
}

void syscall_vfs_readfile(char* filename,char* buffer) {
    __syscall(SYSCALL_VFS_READFILE, filename, buffer);
}

void syscall_vfs_writefile(char* filename,char* buffer,unsigned int size){
    __syscall(SYSCALL_VFS_WRITEFILE, filename, buffer,size);
}

void syscall_sysinfo(struct sysinfo* info){
    __syscall(SYSCALL_SYSINFO,info);
}

int syscall_exec(char *filename,char* args,int is_async){
    return __syscall(SYSCALL_EXEC, filename, args, is_async);
}

uint32_t *syscall_framebuffer(){
    return (uint32_t *)__syscall(SYSCALL_FRAMEBUFFER);
}

void syscall_draw_bitmap(int x,int y,int width,int height,char* bitmap){
    __syscall(SYSCALL_DRAW_BITMAP,x,y,width,height,bitmap);
}

void syscall_vfs_change_path(const char *path){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(path);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_CHANGE_PATH), "r"(ebx) : "memory", "cc");
}

char* syscall_get_arg(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_GET_ARG) : "memory", "cc");
    return (char *)rets;
}

long syscall_clock(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_CLOCK) : "memory", "cc");
    return rets;
}

void syscall_sleep(uint32_t timer){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(timer);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_SLEEP), "r"(ebx) : "memory", "cc");
}

int syscall_vfs_remove_file(char* filename){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(filename);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_VFS_REMOVE_FILE), "r"(ebx) : "memory", "cc");
    return rets;
}

int syscall_vfs_rename(char* filename1,char* filename2){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(filename1);
    uint32_t __arg2 = (uint32_t)(filename2);
    register uint32_t ebx asm("ebx")  = __arg1;
    register uint32_t ecx asm("ecx") = __arg2;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_VFS_RENAME), "r"(ebx), "r"(ecx) : "memory", "cc");
    return rets;
}

void syscall_cp_system(procces_t *pcb){
    __syscall(SYSCALL_CP_SYSTEM,pcb);
}

void *mmap(void *addr, size_t size) {
    return (void *)__syscall(SYSCALL_ALLOC_PAGE, (size + 4095) / 4096);
}

int munmap(void *addr, size_t size) {
    return 0;
}
