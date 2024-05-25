#include "../include/multiboot.h"
#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/description_table.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/timer.h"
#include "../include/task.h"
#include "../include/cmos.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/acpi.h"
#include "../include/syscall.h"
#include "../include/vdisk.h"
#include "../include/pci.h"
#include "../include/pcnet.h"
#include "../include/ide.h"
#include "../include/vfs.h"
#include "../include/fat.h"
#include "../include/iso9660.h"

extern uint32_t end;
extern int status;
uint32_t placement_address = (uint32_t) & end;

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
    while (1){
        if(shell->state == TASK_DEATH){
            printf("\033\n[Task-Check]: Task was throw exception.\036\n");
            printf("Enter any key to restart kernel.> ");
            getc();
            printf("\n");
            reset_kernel();
        }
    }
    return 0;
}

void kernel_main(multiboot_t *multiboot) {

    io_cli();
    vga_install();

    if ((multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1 < 16) {
        printf("[kernel] Minimal RAM amount for CPOS is 16 MB, but you have only %d MB.\n",
               (multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
        while (1) io_hlt();
    }

    initVBE(multiboot);

    printf("[kernel]: VGA driver load success!\n");
    gdt_install();
    idt_install();
    printf("[kernel]: description table config success!\n");
    init_timer(1);
    acpi_install();
    printf("[kernel]: ACPI enable success!\n");
    init_page(multiboot);

    printf("[kernel]: page set success!\n");
    init_sched();
    printf("[kernel]: task load success!\n");
    init_keyboard();
    printf("[kernel]: Keyboard driver load success!\n");
    io_sti();

    //kernel_thread(cur_task,NULL,"CPOS-VBE-SERVICE");

    init_pit();
    init_pci();
    init_vdisk();
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    init_vfs();
    Register_fat_fileSys();
    init_iso9660();
    syscall_install();

    vfs_mount_disk('A','A');
    if(vfs_change_disk('A'))
        printf("[FileSystem]: Change disk win!\n");

    if(pcnet_find_card()){
        //init_pcnet_card();
    } else printf("[kernel]: Cannot found pcnet.\n");

    print_cpu_id();

    clock_sleep(25);

    int pid = kernel_thread(setup_shell, NULL, "CPOS-Shell");
    kernel_thread(check_task,&pid,"CPOS-SHELL-CHECK");

    for (;;) {
        io_hlt();
        clock_sleep(1);
    }
}