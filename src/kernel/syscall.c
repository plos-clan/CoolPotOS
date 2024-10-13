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
#include "../include/memory.h"
#include "../include/timer.h"
#include "../include/panic.h"

typedef struct procces{
    int pid;
    char* cmd;
    FILE* stdout;
    FILE* stderr;
    FILE* stdin;
}procces_t;

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
    if(address == NULL){
        if(get_current()->task_level == TASK_SYSTEM_SERVICE_LEVEL)
            panic_pane("System Service out of memory error.",OUT_OF_MEMORY);
        else if(get_current()->task_level == TASK_APPLICATION_LEVEL)
            task_kill(get_current()->pid);
    }
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
    io_sti();
    return vfs_filesize(ebx);
}

static void syscall_vfs_readfile(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    vfs_readfile(ebx,ecx);
}

static void syscall_vfs_writefile(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    vfs_writefile(ebx,ecx,edx);
}

static void syscall_vfs_chang_path(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    vfs_change_path(ebx);
}

static void* syscall_sysinfo(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    struct sysinfo *info = ebx;
    cpu_t *cpu = get_cpuid();

    memcpy(info->osname,OS_NAME, strlen(OS_NAME)); info->osname[strlen(OS_NAME) + 1] = '\0';
    memcpy(info->kenlname,KERNEL_NAME, strlen(KERNEL_NAME)); info->kenlname[strlen(KERNEL_NAME) + 1] = '\0';
    memcpy(info->cpu_vendor,cpu->vendor, strlen(cpu->vendor)); info->cpu_vendor[strlen(cpu->vendor) + 1] = '\0';
    memcpy(info->cpu_name,cpu->model_name, strlen(cpu->model_name)); info->cpu_name[strlen(cpu->model_name) + 1] = '\0';

    extern uint32_t width,height; //vbe.c

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

uint32_t syscall_exec(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    char* argv = ecx;
    uint32_t pid = user_process(ebx,ebx,argv,TASK_APPLICATION_LEVEL);
    if(!edx){
       if(pid != 0){
           struct task_struct *pi_task = found_task_pid(pid);
           while (pi_task->state != TASK_DEATH);

           get_current()->tty->cx = pi_task->tty->cx;
           get_current()->tty->cy = pi_task->tty->cy;
       }
    }
    return pid;
}

static char* syscall_get_arg(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    char* argv = user_alloc(get_current(), strlen(get_current()->argv) + 1);
    strcpy(argv,get_current()->argv);
    return argv;
}

static uint32_t syscall_clock(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    return get_current()->cpu_clock;
}

static void syscall_sleep(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    clock_sleep(ebx);
}

static int syscall_vfs_remove_file(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    return vfs_delfile(ebx);
}

static int syscall_vfs_rename(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    io_sti();
    return vfs_renamefile(ebx,ecx);
}

static void* syscall_alloc_page(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    size_t page_size = ebx;
    uint32_t i = get_current()->page_alloc_address;
    if(page_size==512) i = (i + (2*1024*1024) - 1) / (2*1024*1024) * (2*1024*1024);
    int ret = i;
    for (;i < ret + (page_size * 0x1000);i += 0x1000) {
        alloc_frame(get_page(i,1,get_current()->pgd_dir,false),0,1);
    }
    get_current()->page_alloc_address = i;

    return ret;
}

static uint32_t *syscall_framebuffer(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    extern uint32_t *screen; //vbe.c
    return screen;
}

static void syscall_draw_bitmap(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    int x = ebx;
    int y = ecx;
    int w = edx;
    int h = esi;
    unsigned int *buffer = (unsigned int *)(edi);
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            drawPixel(i,j,buffer[(j - y) * w + (i - x)]);
        }
    }
}

static void syscall_cp_system(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    procces_t *pcb = ebx;
    int pid = user_process(pcb->cmd[0],"SubProcess",pcb->cmd,TASK_APPLICATION_LEVEL);
    if(pid != -1){
        pcb->pid = pid;
    } else pcb->pid = 0;
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
        [SYSCALL_GET_ARG] = syscall_get_arg,
        [SYSCALL_CLOCK] = syscall_clock,
        [SYSCALL_SLEEP] = syscall_sleep,
        [SYSCALL_VFS_REMOVE_FILE] = syscall_vfs_remove_file,
        [SYSCALL_VFS_RENAME] = syscall_vfs_rename,
        [SYSCALL_ALLOC_PAGE] = syscall_alloc_page,
        [SYSCALL_FRAMEBUFFER] = syscall_framebuffer,
        [SYSCALL_DRAW_BITMAP] = syscall_draw_bitmap,
        [SYSCALL_CP_SYSTEM] = syscall_cp_system,
};

typedef size_t (*syscall_t)(size_t, size_t, size_t, size_t, size_t);
extern int can_sche; //进程调度器 0 禁用 | 1 启用

size_t syscall() {
    io_cli();
   // can_sche = 0;
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
    can_sche = 1;
    io_sti();
    return eax;
}

void syscall_install(){
    extern void asm_syscall_handler();
    idt_use_reg(31,asm_syscall_handler);
}