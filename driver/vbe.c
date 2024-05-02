#include "../include/graphics.h"
#include "../include/io.h"

unsigned int vesa_fb_width, vesa_fb_height, vesa_fb_bpp, vesa_fb_pitch;
uint8_t *vesa_fb_addr;
extern int status;

void putPix(unsigned int x, unsigned int y, uint32_t color) {
    if (status != 1)
        return;
    color_rgba col;
    col.r = color >> 16;
    col.g = color >> 8;
    col.b = color;
    col.a = 0xFF;

    if (x >= vesa_fb_width || y >= vesa_fb_height)
        return;
    unsigned where = x * (vesa_fb_bpp / 8) + y * vesa_fb_pitch;
    vesa_fb_addr[where + 0] = col.b;
    vesa_fb_addr[where + 1] = col.g;
    vesa_fb_addr[where + 2] = col.r;
}

void initVBE(multiboot_t *mboot) {
    vesa_fb_addr = (uint8_t * )(
    int)(mboot->framebuffer_addr);
    vesa_fb_pitch = mboot->framebuffer_pitch;
    vesa_fb_bpp = mboot->framebuffer_bpp;
    vesa_fb_width = mboot->framebuffer_width;
    vesa_fb_height = mboot->framebuffer_height;

    printf("[\035VBE driver\036]: Framebuffer address: 0x%08x\n",vesa_fb_addr);
    printf("[\035VBE driver\036]: Screen Size: width:%d - height:%d\n",mboot->framebuffer_width,mboot->framebuffer_height);
    // TODO: insert into devtable
}


void vbe_putchar(char c) {

}

void vbe_clear() {

}
