#include "syscall.h"
#include "io.h"
#include "pcb.h"
#include "scheduler.h"
#include "description_table.h"
#include "klog.h"
#include "keyboard.h"
#include "krlibc.h"
#include "user_malloc.h"

static uint32_t syscall_putc(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    get_current_proc()->tty->putchar(get_current_proc()->tty,(int)ebx);
    return 0;
}

static uint32_t syscall_print(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    get_current_proc()->tty->print(get_current_proc()->tty, (const char *) ebx);
    return 0;
}

static uint32_t syscall_getch(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    return kernel_getch();
}

static uint32_t syscall_alloc_page(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    return (uint32_t) user_malloc(ebx);
}

static uint32_t syscall_free(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    user_free((void*)ebx);
    return 0;
}

static uint32_t syscall_exit(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    int exit_code = ebx;
    pcb_t *pcb = get_current_proc();
    kill_proc(pcb);
    while (1);
    /*
     * 将该进程流程阻塞, 等待调度器下一次调度
     * (因为这时有关该进程的所有上下文信息以及内存都已经被释放, 如果iret返回后则会直接发生#PF错误, 故插入死循环阻塞)
     */
    return 0;
}

static uint32_t syscall_get_arg(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    pcb_t *pcb = get_current_proc();
    strcpy((char *) USER_AREA_START, pcb->cmdline);
    pcb->user_cmdline = (char*)USER_AREA_START;
    return (uint32_t)pcb->user_cmdline;
}

static uint32_t syscall_posix_open(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi) {
    void *r = vfs_open((const char*)ebx);
    return (uint32_t) (r == NULL ? -1 : r);
}

static uint32_t syscall_posix_close(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi) {
    return (uint32_t ) vfs_close((void*)ebx);
}

static uint32_t syscall_posix_read(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi) {
    vfs_node_t file = (vfs_node_t) ebx;
    void* buf = (void*) ecx;
    size_t count = (size_t)edx;
    if(count == 0) return 0;
    io_sti();
    if(vfs_read(file,buf,0,count) == -1){
        return -1;
    }
    io_cli();
    return file->size < count ? file->size : count;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = syscall_putc,
        [SYSCALL_PRINT] = syscall_print,
        [SYSCALL_GETCH] = syscall_getch,
        [SYSCALL_ALLOC_PAGE] = syscall_alloc_page,
        [SYSCALL_FREE] = syscall_free,
        [SYSCALL_EXIT] = syscall_exit,
        [SYSCALL_GET_ARG] = syscall_get_arg,
        [SYSCALL_POSIX_OPEN] = syscall_posix_open,
        [SYSCALL_POSIX_CLOSE] = syscall_posix_close,
        [SYSCALL_POSIX_READ] = syscall_posix_read,
};

size_t syscall() { //由 asmfunc.c/asm_syscall_handler调用
    io_cli();
    // disable_scheduler();
    volatile size_t eax, ebx, ecx, edx, esi, edi;
    __asm__("mov %%eax, %0\n\t" : "=r"(eax));
    __asm__("mov %%ebx, %0\n\t" : "=r"(ebx));
    __asm__("mov %%ecx, %0\n\t" : "=r"(ecx));
    __asm__("mov %%edx, %0\n\t" : "=r"(edx));
    __asm__("mov %%esi, %0\n\t" : "=r"(esi));
    __asm__("mov %%edi, %0\n\t" : "=r"(edi));
    if (0 <= eax && eax < MAX_SYSCALLS && syscall_handlers[eax] != NULL) {
        eax = ((syscall_t)syscall_handlers[eax])(ebx, ecx, edx, esi, edi);
    } else {
        eax = -1;
    }
    enable_scheduler();
    io_sti();
    return eax;
}

void setup_syscall(){
    idt_use_reg(31, (uint32_t) asm_syscall_handler);
    klogf(true,"Register syscall interrupt service.\n");
}