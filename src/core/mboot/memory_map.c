#include "multiboot.h"
#include "ctypes.h"
#include "klog.h"
#include "io.h"

uint32_t phy_mem_size;

void check_memory(multiboot_t *multiboot){
    if ((multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1 < 30) {
        printk("CP_Kernel bootloader panic info:\n");
        printk("\033ff0000;Minimal RAM amount for CP_Kernel is 30 MB, but you have only %d MB.\033c6c6c6;\n",
               (multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1);
        printk("Please check your memory size, and restart CP_Kernel.\n");
        while (1) io_hlt();
    }
    phy_mem_size = (multiboot->mem_upper + multiboot->mem_lower) / 1024;
}

void show_memory_map(multiboot_t *mboot) {
    uint32_t mmap_addr = mboot->mmap_addr;
    uint32_t mmap_length = mboot->mmap_length;

    /*
    mmap_entry_t *mmap = (mmap_entry_t *) mmap_addr;
    for (mmap = (mmap_entry_t *) mmap_addr; (uint32_t) mmap < mmap_addr + mmap_length; mmap++) {
        if(mmap->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE)
            logkf("mmap: %08x ACPI\n",(uint32_t) mmap->base_addr_low);
        else if(mmap->type == MULTIBOOT_MEMORY_BADRAM)
            logkf("mmap: %08x BAD\n",(uint32_t) mmap->base_addr_low);
        else if(mmap->type == MULTIBOOT_MEMORY_INFO)
            logkf("mmap: %08x INFO\n",(uint32_t) mmap->base_addr_low);
        else if(mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
            logkf("mmap: %08x AVAILABLE\n",(uint32_t) mmap->base_addr_low);
        else if(mmap->type == MULTIBOOT_MEMORY_NVS)
            logkf("mmap: %08x NVS\n",(uint32_t) mmap->base_addr_low);
        else if(mmap->type == MULTIBOOT_MEMORY_RESERVED)
            logkf("mmap: %08x RESERVED\n",(uint32_t) mmap->base_addr_low);
        else logkf("mmap: %08x UNKNOWN\n",(uint32_t) mmap->base_addr_low);

    }*/
}

void init_mmap(multiboot_t *multiboot){
    phy_mem_size = (multiboot->mem_upper + multiboot->mem_lower) / 1024;
    show_memory_map(multiboot);
}