#include "../include/multiboot.h"
#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/description_table.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/timer.h"
#include "../include/task.h"
#include "../include/fpu.h"
#include "../include/cmos.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/acpi.h"
#include "../include/syscall.h"
#include "../include/vdisk.h"
#include "../include/pci.h"
#include "../include/pcnet.h"
#include "../include/ide.h"
#include "../include/ahci.h"
#include "../include/vfs.h"
#include "../include/panic.h"
#include "../include/mouse.h"
#include "../include/i2c.h"
#include "../include/desktop.h"
#include "../include/soundtest.h"

#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

extern uint32_t end;
extern int status;
extern vdisk vdisk_ctl[10];
extern bool hasFS;
uint32_t placement_address = (uint32_t) & end;

uint32_t phy_mem_size;

void reset_kernel(){
    printf("Restart %s for x86...\n",OS_NAME);
    kill_all_task();
    clock_sleep(10);
    power_reset();
}

void shutdown_kernel(){
    printf("Shutdown %s for x86...\n",OS_NAME);
    kill_all_task();
    clock_sleep(10);
    power_off();
}

uint32_t memory_all(){
    return 0;
}

int check_task(int *pid){
    struct task_struct *shell = found_task_pid(*pid);

    if(shell == NULL){
        printf("\n[Task-Check]: Task was throw exception.\n");
        printf("Enter any key to restart kernel.> ");
        getc();
        printf("\n");
        reset_kernel();
    }

    while (1){
        if(shell->state == TASK_DEATH){
            printf("\n[Task-Check]: Task was throw exception.\n");
            printf("Enter any key to restart kernel.> ");
            getc();
            printf("\n");
            reset_kernel();
        }
    }
    return 0;
}

int check_task_usershell(int *pid){
    while (1);
    struct task_struct *shell = found_task_pid(*pid);
    while (1){
        if(shell->state == TASK_DEATH){
            io_sti();
            screen_clear();
            int pid = kernel_thread(setup_shell,NULL,"CPOS-Shell");
            kernel_thread(check_task,&pid,"CPOS-CK");
            break;
        }
    }

    while (1);

    return 0;
}

void cursor_task(){
    while (1){
        tty_t *tty = get_current()->tty;
        draw_rect_tty(tty,tty->x,tty->y,tty->x + 10,tty->y + 8,tty->color);
        clock_sleep(1);
    }
}

void kernel_main(multiboot_t *multiboot) {

    io_cli();
    vga_install();

    initVBE(multiboot);

    /*
    if ((multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1 < 3071) {
        printf("[kernel]: Minimal RAM amount for CP_Kernel is 3071 MB, but you have only %d MB.\n",
               (multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
        while (1) io_hlt();
    }
     */

    phy_mem_size = (multiboot->mem_upper + multiboot->mem_lower) / 1024;
    char* cmdline = multiboot->cmdline;

    if(cmdline != NULL){
       // printf("Multiboot command line: %s\n",cmdline);
    }

    logkf("\n\n\n");
    printf("%s OS Version: %s (GRUB Multiboot) on an i386.\n",KERNEL_NAME,OS_VERSION);
    printf("Memory Size: %dMB | ",(multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
    printf("Video Resolution: %d x %d\n",multiboot->framebuffer_width,multiboot->framebuffer_height);
    gdt_install();
    idt_install();
    init_timer(1);
    acpi_install();
    init_page(multiboot);

    init_sched();
    //fpu_setup();

    int pid_cur = kernel_thread(cursor_task,NULL,"System-Cur");
    // klogf(pid != -1,"Launch cursor service\n");

    init_keyboard();

    init_pit();
    io_sti();

    init_vdisk();
    init_vfs();

    init_pci();

    ide_init();
    ahci_init();

    syscall_install();

    char disk_id = '0';

    for (int i = 0; i < 10; i++) {
        if (vdisk_ctl[i].flag) {
            vdisk vd = vdisk_ctl[i];
            char id = i + ('A');
            if(vfs_mount_disk(id,id)){
                disk_id = id;
                klogf(true,"Mount disk: %c\n",disk_id);
            }
        }
    }
    hasFS = false;
    if(disk_id != '0'){
        if(vfs_change_disk(get_current(),disk_id)){
            extern char root_disk;
            root_disk = disk_id;
            klogf(true,"Chang default mounted disk.\n");
            hasFS = true;
        }
    } else klogf(false,"Unable to find available IDE devices.\n");

    bool pcnet_init = pcnet_find_card();
    klogf(pcnet_init,"Enable network device pcnet.\n");
    if(pcnet_init){
        //init_pcnet_card();
    }
    init_eh();
    klogf(true,"Kernel load done!\n");

    io_sti();

    clock_sleep(25);

    //vfs_change_path("sys");
    //int pid = user_process("csp.bin","CSP","",TASK_SYSTEM_SERVICE_LEVEL);


    vfs_change_path("apps");
    //klogf(user_process("init.bin","InitService",true) != -1,"Init service process init.\n");
    int pid = user_process("shell.bin","UserShell","shell.bin -d shell -c -SsS",TASK_SYSTEM_SERVICE_LEVEL);
    kernel_thread(check_task_usershell,&pid,"CTU");

   // int pid = kernel_thread(setup_shell,NULL,"CPOS-Shell");
   // klogf(pid != -1,"Launch kernel shell.\n");
    //kernel_thread(check_task,&pid,"CPOS-CK");

    //panic_pane("System out of memory error!",OUT_OF_MEMORY);


    for (;;) {
        io_hlt();
        clock_sleep(1);
    }
}