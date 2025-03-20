#include "ctypes.h"
#include "io.h"
#include "klog.h"
#include "multiboot.h"

uint32_t phy_mem_size;

void check_memory(multiboot_t *multiboot) {
    if ((multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1 < 30) { //计算总内存大小
        printk("CP_Kernel bootloader panic info:\n");
        printk("\033ff0000;Minimal RAM amount for CP_Kernel is 30 MB, but you have only %d "
               "MB.\033c6c6c6;\n",
               (multiboot->mem_upper + multiboot->mem_lower) / 1024 + 1); //内存不足，输出错误信息
        printk("Please check your memory size, and restart CP_Kernel.\n");
        while (1)
            io_hlt(); //死循环阻塞线程，防止系统启动
    }
    phy_mem_size = (multiboot->mem_upper + multiboot->mem_lower) / 1024; //记录总内存大小
}

void show_memory_map(multiboot_t *mboot) {
    uint32_t mmap_addr   = mboot->mmap_addr;   //获取内存起始地址
    uint32_t mmap_length = mboot->mmap_length; //获取内存总长度

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

void init_mmap(multiboot_t *multiboot) {
    phy_mem_size = (multiboot->mem_upper + multiboot->mem_lower) / 1024; //计算总内存大小
    show_memory_map(multiboot);                                          //显示内存映射信息
}