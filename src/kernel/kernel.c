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
#include "../include/panic.h"
#include "../include/mouse.h"
#include "../include/desktop.h"
#include "../include/soundtest.h"

#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

extern uint32_t end;
extern int status;
extern vdisk vdisk_ctl[10];
extern bool hasFS;
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
            printf("\n[Task-Check]: Task was throw exception.\n");
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


    char* cmdline = multiboot->cmdline;
    if(cmdline != NULL){
        printf("Multiboot command line: %s\n",cmdline);
    }

    printf("CPOS_Kernel %s (GRUB Multiboot) on an i386.\n",OS_VERSION);
    printf("Memory Size: %dMB\n",(multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
    gdt_install();
    idt_install();
    init_timer(1);
    acpi_install();
    init_page(multiboot);

    init_sched();
    //proc_install();
    init_keyboard();

    init_pit();
    io_sti();
    init_pci();
    init_vdisk();
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    init_vfs();
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
    init_eh();
    hasFS = false;
    if(disk_id != '0'){
        if(vfs_change_disk(disk_id)){
            klogf(true,"Chang default mounted disk.\n");
            hasFS = true;
        }
    } else klogf(false,"Unable to find available IDE devices.\n");

    bool pcnet_init = pcnet_find_card();
    klogf(pcnet_init,"Enable network device pcnet.\n");
    if(pcnet_init){
        //init_pcnet_card();
    }
    klogf(true,"Kernel load done!\n");
    kernel_thread(sound_test,NULL,"Sound");

    io_sti();

    clock_sleep(25);

   // vfs_change_path("apps");
    //klogf(user_process("service.bin","Service") != -1,"Service base process init.\n");
    //klogf(user_process("init.bin","Init") != -1,"Init base process init.\n");

    int pid = kernel_thread(setup_shell,NULL,"CPOS-Shell");
    klogf(pid != -1,"Launch kernel shell.\n");
    kernel_thread(check_task,&pid,"CPOS-CK");


    //panic_pane("Proccess out of memory error!",OUT_OF_MEMORY);

    for (;;) {
        io_hlt();
        clock_sleep(1);
    }
}