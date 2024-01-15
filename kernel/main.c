#include "../include/vga.h"
#include "../include/io.h"
#include "../include/description_table.h"
#include "../include/multiboot.h"

void kernel_main(){
    vga_install();
    install_idt();
    install_gdt();

    

    asm("sti");
    printf("CrashPowerDOS for x86 [Version %s]",VERSION);

    for(;;) io_hlt();
}