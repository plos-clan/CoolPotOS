#include "syscall.h"
#include "description_table.h"
#include "io.h"
#include "keyboard.h"
#include "klog.h"
#include "krlibc.h"
#include "pcb.h"
#include "scheduler.h"
#include "user_malloc.h"

static uint32_t syscall_putc(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    get_current_proc()->tty->putchar(get_current_proc()->tty, (int)ebx);
    return 0;
}

static uint32_t syscall_print(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                              uint32_t edi) {
    get_current_proc()->tty->print(get_current_proc()->tty, (const char *)ebx);
    return 0;
}

static uint32_t syscall_getch(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                              uint32_t edi) {
    io_sti();
    return kernel_getch();
}

static uint32_t syscall_alloc_page(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                   uint32_t edi) {
    return (uint32_t)user_malloc(get_current_proc(), ebx);
}

static uint32_t syscall_free(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    user_free((void *)ebx);
    return 0;
}

static uint32_t syscall_exit(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    int    exit_code = ebx;
    pcb_t *pcb       = get_current_proc();
    kill_proc(pcb);
    while (1)
        ;
    /*
     * 将该进程流程阻塞, 等待调度器下一次调度
     * (因为这时有关该进程的所有上下文信息以及内存都已经被释放, 如果iret返回后则会直接发生#PF错误, 故插入死循环阻塞)
     */
    return 0;
}

static uint32_t syscall_get_arg(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                uint32_t edi) {
    pcb_t *pcb = get_current_proc();
    strcpy((char *)USER_AREA_START, pcb->cmdline);
    pcb->user_cmdline = (char *)USER_AREA_START;
    return (uint32_t)pcb->user_cmdline;
}

typedef enum oflags {
    O_RDONLY,
    O_WRONLY,
    O_RDWR,
    O_CREAT = 4
} oflags_t;

static uint32_t syscall_posix_open(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                   uint32_t edi) {
    io_sti();
    char *name = (char *)ebx;
    if (ecx & O_CREAT) {
        int status = vfs_mkfile(name);
        if (status == -1) goto error;
    }
    vfs_node_t r = vfs_open(name);
    if (r == NULL) goto error;
    for (int i = 3; i < 255; i++) {
        if (get_current_proc()->file_table[i] == NULL) {
            cfile_t file                      = kmalloc(sizeof(cfile_t));
            file->handle                      = r;
            file->pos                         = 0;
            file->flags                       = ecx;
            get_current_proc()->file_table[i] = file;
            return i;
        }
    }
error:
    return -1;
}

static uint32_t syscall_posix_close(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                    uint32_t edi) {
    for (int i = 3; i < 255; i++) {
        if (i == ebx) {
            cfile_t file = get_current_proc()->file_table[i];
            if (file == NULL) return -1;
            vfs_close(file->handle);
            kfree(file);
            return (uint32_t)(get_current_proc()->file_table[i] = NULL);
        }
    }
    return -1;
}

static uint32_t syscall_posix_read(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                   uint32_t edi) {
    io_sti();
    if (ebx < 0 || ebx == 1 || ebx == 2) return -1;
    cfile_t file = get_current_proc()->file_table[ebx];
    if (file == NULL) return -1;
    if (file->flags == O_WRONLY) return -1;
    char *buffer = kmalloc(file->handle->size);
    if (vfs_read(file->handle, buffer, 0, file->handle->size) == -1) return -1;
    int ret = 0; // 记录到底读了多少个字节
    io_cli();
    char *filebuf = (char *)buffer; // 文件缓冲区
    char *retbuf  = (char *)ecx;    // 接收缓冲区

    if (edx > file->handle->size) {
        memcpy(retbuf, filebuf, file->handle->size);
        ret = file->pos += file->handle->size;
    } else {
        memcpy(retbuf, filebuf, edx);
        ret = file->pos += edx;
    }
    kfree(buffer);
    return ret;
}

static uint32_t syscall_posix_sizex(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                    uint32_t edi) {
    if (ebx < 0 || ebx == 1 || ebx == 2) return -1;
    cfile_t file = get_current_proc()->file_table[ebx];
    if (file == NULL) return -1;
    return file->handle->size;
}

static uint32_t syscall_realloc(uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi,
                                uint32_t edi) {
    return (uint32_t)user_realloc(get_current_proc(), (void *)ebx, ecx);
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_PUTC]        = syscall_putc,
    [SYSCALL_PRINT]       = syscall_print,
    [SYSCALL_GETCH]       = syscall_getch,
    [SYSCALL_ALLOC_PAGE]  = syscall_alloc_page,
    [SYSCALL_FREE]        = syscall_free,
    [SYSCALL_EXIT]        = syscall_exit,
    [SYSCALL_GET_ARG]     = syscall_get_arg,
    [SYSCALL_POSIX_OPEN]  = syscall_posix_open,
    [SYSCALL_POSIX_CLOSE] = syscall_posix_close,
    [SYSCALL_POSIX_READ]  = syscall_posix_read,
    [SYSCALL_POSIX_SIZEX] = syscall_posix_sizex,
    [SYSCALL_POSIX_WRITE] = 0,
    [SYSCALL_REALLOC]     = syscall_realloc,
};

size_t syscall() { // 由 asmfunc.c/asm_syscall_handler调用
    io_cli();
    disable_scheduler();
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
    return eax;
}

void setup_syscall() {
    idt_use_reg(31, (uint32_t)asm_syscall_handler);
    klogf(true, "Register syscall interrupt service.\n");
}