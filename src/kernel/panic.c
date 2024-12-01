#include "../include/panic.h"
#include "../include/task.h"
#include "../include/isr.h"
#include "../include/io.h"
#include "../include/printf.h"
#include "../include/bmp.h"
#include "../include/vfs.h"
#include "../include/timer.h"
#include "../include/graphics.h"
#include "../include/acpi.h"

extern struct task_struct *current;
extern uint32_t *screen;
extern uint32_t back_color;
extern uint32_t color;
extern int32_t cx;
extern int32_t cy;
extern uint32_t c_width;
extern uint32_t c_height;
Bmp *panic_bmp;

static GP_13(registers_t *reg){
    io_cli();
    printf("throw #GP 13 error.\n");
    if(current->pid == 0){
        printf("Kernel PANIC(#GP), Please restart your CPOS Kernel.\n");
        while(1) io_hlt();
    }else {
        task_kill(current->pid);
    }
    io_sti();
}

static UD_6(registers_t *reg){
    if(current->pid == 0){
        printf("Kernel PANIC(#UD), Please restart your CPOS Kernel.\n");
        while(1) io_hlt();
    }else {
        task_kill(current->pid);
    }
}

static DE_0(registers_t *reg){
    if(current->pid == 0){
        printf("Kernel PANIC(#DE), Please restart your CPOS Kernel.\n");
        while(1) io_hlt();
    }else {
        task_kill(current->pid);
    }
}

void panic_pane(char* msg,enum PANIC_TYPE type){
    io_cli();
    extern int can_sche;
    can_sche = 0;
    kill_all_task();
    back_color = 0x1e90ff;
    color = 0xffffff;
    screen_clear();
    if(panic_bmp != NULL){
        display(panic_bmp,0,0,true);
    } else klogf(false,"Cannot draw panic image.\n");

    cx = 10;
    cy = c_height - 10;

    register uint32_t ebx asm("ebx");
    register uint32_t ecx asm("ecx");

    printf("%s: %s\n",type == ILLEGAL_KERNEL_STATUS ? "ILLEGAL_KERNEL_STATUS" : (type == OUT_OF_MEMORY ? "OUT_OF_MEMORY" :(type == KERNEL_PAGE_FAULT ? "KERNEL_PAGE_FAULT" : "UNKOWN_ERROR")),msg);
    cx = 10;
    printf("EAX: %08x | EBX: %08x | ECX: %08x | ESP: %08x | EBP: %08x | EDI: %08x | ESI: %08x\n",current,ebx,ecx,current->context.esp
            ,current->context.ebp,current->context.edi,current->context.esi);
    printf("\n\n");
    cx = 10;
    printf("Restarting your system...\n");

    io_sti();
    for (int i = 90; i < 1200; i++) {
        for (int j = 650; j < 670; j++) {
            drawPixel(i,j,0xffffff);
        }
        sleep(1);
    }
    sleep(100);
    power_reset();
    while (1) io_hlt();
}

void init_eh(){
    register_interrupt_handler(13,GP_13);
    register_interrupt_handler(6,UD_6);
    register_interrupt_handler(0,DE_0);
    panic_bmp = NULL;
    uint32_t size = vfs_filesize("panic.bmp");
    if(size == -1){
        klogf(false,"Enable graphics user interface panic.\n");
    } else{
        Bmp *bmp = kmalloc(size);
        vfs_readfile("panic.bmp",bmp);
        panic_bmp = bmp;
        klogf(true,"Enable graphics user interface panic.\n");
    }
}