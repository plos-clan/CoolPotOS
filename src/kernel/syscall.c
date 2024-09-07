#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/shell.h"
#include "../include/heap.h"
#include "../include/keyboard.h"
#include "../include/vfs.h"
#include "../include/cmos.h"

extern uint32_t phy_mem_size;
extern unsigned int PCI_NUM;

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

static int syscall_vfs_filesize(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    return vfs_filesize(ebx);
}

static void syscall_vfs_readfile(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    vfs_readfile(ebx,ecx);
}

static void syscall_vfs_writefile(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    vfs_writefile(ebx,ecx,edx);
}

static void syscall_vfs_chang_path(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    vfs_change_path(ebx);
}

struct sysinfo{
    char* osname;
    char* kenlname;
    char* cpu_vendor;
    char* cpu_name;
    uint32_t phy_mem_size;
    uint32_t pci_device;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t year;
    uint32_t mon;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
};

static void* syscall_sysinfo(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    struct sysinfo *info = (struct sysinfo *) user_alloc(get_current(), sizeof(struct sysinfo));
    cpu_t *cpu = get_cpuid();
    char *os_name = (char *)user_alloc(get_current(), strlen(OS_NAME));
    char *kernel = (char *)user_alloc(get_current(), strlen(KERNEL_NAME));

    char *cpu_vendor = (char *)user_alloc(get_current(), strlen(cpu->vendor));
    char *cpu_name = (char *)user_alloc(get_current(), strlen(cpu->model_name));
    memcpy(os_name,OS_NAME, strlen(OS_NAME));
    memcpy(kernel,KERNEL_NAME, strlen(KERNEL_NAME));
    memcpy(cpu_vendor,cpu->vendor, strlen(cpu->vendor));
    memcpy(cpu_name,cpu->model_name, strlen(cpu->model_name));

    extern uint32_t width,height; //vbe.c

    info->osname = os_name;
    info->kenlname = kernel;
    info->cpu_vendor = cpu_vendor;
    info->cpu_name = cpu_name;
    info->phy_mem_size = phy_mem_size;
    info->pci_device = PCI_NUM;
    info->frame_width = width;
    info->frame_height = height;
    info->year = get_year();
    info->mon = get_mon();
    info->day = get_day_of_month();
    info->hour = get_hour();
    info->min = get_min();
    info->sec = get_sec();

    kfree(cpu);

    return info;
}

static int syscall_exec(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    int pid = user_process(ebx,ebx);
    return pid;
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
        [SYSCALL_VFS_FILESIZE] = syscall_vfs_filesize,
        [SYSCALL_VFS_READFILE] = syscall_vfs_readfile,
        [SYSCALL_VFS_WRITEFILE] = syscall_vfs_writefile,
        [SYSCALL_SYSINFO] = syscall_sysinfo,
        [SYSCALL_EXEC] = syscall_exec,
        [SYSCALL_CHANGE_PATH] = syscall_vfs_chang_path,
};

typedef size_t (*syscall_t)(size_t, size_t, size_t, size_t, size_t);

size_t syscall() {
    io_cli();
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