#include "../include/syscall.h"

void syscall_print(char* c){
    uint32_t rets;
    uint32_t __arg1 = c;
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PRINT), "r"(ebx) : "memory", "cc");
}

void syscall_putchar(char c){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(c);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PUTC), "r"(ebx) : "memory", "cc");
}

char syscall_getch(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_GETCH) : "memory", "cc");
    return rets;
}

void* syscall_malloc(size_t size){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(size);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_MALLOC), "r"(ebx) : "memory", "cc");
    return rets;
}

void syscall_free(void *ptr){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(ptr);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_FREE), "r"(ebx) : "memory", "cc");
}

void syscall_exit(int code){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(code);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_EXIT), "r"(ebx)  : "memory", "cc");\
}

void syscall_g_clean(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_G_CLEAN) : "memory", "cc");
}

void syscall_get_cd(char *buffer){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(buffer);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_GET_CD), "r"(ebx) : "memory", "cc");
}

int syscall_vfs_filesize(char* filename){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(filename);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_VFS_FILESIZE), "r"(ebx) : "memory", "cc");
    return rets;
}

void syscall_vfs_readfile(char* filename,char* buffer){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(filename);
    uint32_t __arg2 = (uint32_t)(buffer);
    register uint32_t ebx asm("ebx") = __arg1;
    register uint32_t ecx asm("ecx") = __arg2;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_VFS_READFILE), "r"(ebx), "r"(ecx) : "memory", "cc");
}

void syscall_vfs_writefile(char* filename,char* buffer,unsigned int size){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(filename);
    uint32_t __arg2 = (uint32_t)(buffer);
    uint32_t __arg3 = (uint32_t)(size);
    register uint32_t ebx asm("ebx") = __arg1;
    register uint32_t ecx asm("ecx") = __arg2;
    register uint32_t edx asm("edx") = __arg3;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_VFS_FILESIZE), "r"(ebx), "r"(ecx), "r"(edx) : "memory", "cc");
}