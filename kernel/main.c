#include "../include/vga.h"
#include "../include/io.h"
#include "../include/description_table.h"
#include "../include/multiboot.h"
#include "../include/memory.h"

extern uint32_t end;
uint32_t placement_address = (uint32_t) & end;

typedef struct 
{
    char* data;
}DEBUG;


void kernel_main(unsigned long magic,struct multiboot_info *mbi){

    vga_install();
    install_idt();
    install_gdt();

    init_page();

    asm("sti");

    printf("CrashPowerDOS for x86 [Version %s] \n",VERSION);

    

    for(;;) io_hlt();
}