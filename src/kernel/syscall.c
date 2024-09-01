#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/shell.h"
#include "../include/heap.h"
#include "../include/keyboard.h"

static void syscall_puchar(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    printf("%c",ebx);
}

static void syscall_print(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    printf("%s",ebx);
}

static char syscall_getch(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    char c = kernel_getch();
    return c;
}

static void syscall_exit(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    task_kill(get_current()->pid);
    printf("PID[%d] exit code: %d",get_current()->pid,ebx);
}

static void* syscall_malloc(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    void* address = user_alloc(get_current(),ebx);
    return address;
}

static void syscall_free(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    use_free(get_current(),ebx);
}

static void syscall_g_clean(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
     screen_clear();
}

static void syscall_get_cd(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    char* buf = ebx;
    extern bool hasFS;
    if(hasFS) vfs_getPath(buf);
    else buf = "nofs";
}

void *sycall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = syscall_puchar,
        [SYSCALL_PRINT] = syscall_print,
        [SYSCALL_GETCH] = syscall_getch,
        [SYSCALL_MALLOC] = syscall_malloc,
        [SYSCALL_FREE] = syscall_free,
        [SYSCALL_EXIT] = syscall_exit,
        [SYSCALL_G_CLEAN] = syscall_g_clean,
        [SYSCALL_GET_CD] = syscall_get_cd,
};

typedef size_t (*syscall_t)(size_t, size_t, size_t, size_t, size_t);

size_t syscall() {
    volatile size_t eax, ebx, ecx, edx, esi, edi;
    asm("mov %%eax, %0\n\t" : "=r"(eax));
    asm("mov %%ebx, %0\n\t" : "=r"(ebx));
    asm("mov %%ecx, %0\n\t" : "=r"(ecx));
    asm("mov %%edx, %0\n\t" : "=r"(edx));
    asm("mov %%esi, %0\n\t" : "=r"(esi));
    asm("mov %%edi, %0\n\t" : "=r"(edi));
    if (0 <= eax && eax < MAX_SYSCALLS && sycall_handlers[eax] != NULL) {
        eax = ((syscall_t)sycall_handlers[eax])(ebx, ecx, edx, esi, edi);
    } else {
        eax = -1;
    }
    return eax;
}

void syscall_install(){
    extern void asm_syscall_handler();
    idt_use_reg(31,asm_syscall_handler);
}