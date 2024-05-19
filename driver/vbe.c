#include "../include/graphics.h"
#include "../include/memory.h"
#include "../include/io.h"

extern page_directory_t *kernel_directory;
uint32_t video_base, video_size;

svga_mode_info_t* svga_mode_get_info(multiboot_t *sys_multiboot_info,uint16_t mode) {
    return (svga_mode_info_t *) sys_multiboot_info->vbe_mode_info;
}
/*
uint32_t svga_map_fb(uint32_t real_addr, uint32_t fb_length) {
    int i = 0;
    uint32_t fb_addr;

    // Align framebuffer length to page boundaries
    fb_length += 0x1000;
    fb_length &= 0x0FFFF000;

    // Map enough framebuffer
    for(i = 0xD0000000; i < 0xD0000000 + fb_length; i += 0x1000) {
        page_t* page = paging_get_page(i, true, kernel_directory);

        fb_addr = (i & 0x0FFFF000) + real_addr;

        page->present = 1;
        page->rw = 1;
        page->user = 1;
        page->frame = fb_addr >> 12;
    }

    // Convert the kernel directory addresses to physical if needed
    for(i = 0x340; i < 0x340 + (fb_length / 0x400000); i++) {
        uint32_t physAddr = kernel_directory->tablesPhysical[i];

        if((physAddr & 0xC0000000) == 0xC0000000) {
            physAddr &= 0x0FFFFFFF; // get rid of high nybble

            kernel_directory->tablesPhysical[i] = physAddr;
        }
    }

    return 0xD0000000;
}
 */

int isVBEDisplayMode(uint16_t vbe_mode_info) {
    if (vbe_mode_info & (1 << 12)) {
        return 1;
    } else {
        return 0;
    }
}

void initVBE(multiboot_t *mboot) {
    svga_mode_info_t *vbe_info = svga_mode_get_info(mboot,SVGA_DEFAULT_MODE);
    if(vbe_info->bpp == 32 || vbe_info->bpp == 16){
        printf("VBE LOAD SUCCESS!");
    }

    printf("[VBE]: Bass: %08x | PITCH: %d | HEIGHT: %d\n",vbe_info->physbase,
           vbe_info->pitch,
           vbe_info->screen_height);

    //video_base = svga_map_fb(svga_mode_info->physbase, svga_mode_info->pitch * svga_mode_info->screen_height);

    while (1) io_hlt();
}
